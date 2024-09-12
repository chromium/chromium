// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_unittest_helpers.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace webrtc_event_logging {

#if BUILDFLAG(IS_WIN)
#define NumberToStringType base::NumberToWString
#else
#define NumberToStringType base::NumberToString
#endif

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

using BrowserContext = content::BrowserContext;
using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;
using MockRenderProcessHost = content::MockRenderProcessHost;
using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;
using RenderProcessHost = content::RenderProcessHost;

using Compression = WebRtcEventLogCompression;

namespace {

#if !BUILDFLAG(IS_ANDROID)

auto SaveFilePathTo(std::optional<base::FilePath>* output) {
  return [output](PeerConnectionKey ignored_key, base::FilePath file_path,
                  int output_period_ms = 0) { *output = file_path; };
}

auto SaveKeyAndFilePathTo(std::optional<PeerConnectionKey>* key_output,
                          std::optional<base::FilePath>* file_path_output) {
  return [key_output, file_path_output](PeerConnectionKey key,
                                        base::FilePath file_path) {
    *key_output = key;
    *file_path_output = file_path;
  };
}

const int kMaxActiveRemoteLogFiles =
    static_cast<int>(kMaxActiveRemoteBoundWebRtcEventLogs);
const int kMaxPendingRemoteLogFiles =
    static_cast<int>(kMaxPendingRemoteBoundWebRtcEventLogs);
const char kSessionId[] = "session_id";

base::Time GetLastModificationTime(const base::FilePath& file_path) {
  base::File::Info file_info;
  if (!base::GetFileInfo(file_path, &file_info)) {
    EXPECT_TRUE(false);
    return base::Time();
  }
  return file_info.last_modified;
}

#endif

// Common default/arbitrary values.
constexpr int kLid = 478;
constexpr size_t kWebAppId = 42;
constexpr int kFrameId = 57;

PeerConnectionKey GetPeerConnectionKey(RenderProcessHost* rph, int lid) {
  const BrowserContext* browser_context = rph->GetBrowserContext();
  const auto browser_context_id = GetBrowserContextId(browser_context);
  return PeerConnectionKey(rph->GetID(), lid, browser_context_id, kFrameId);
}

bool CreateRemoteBoundLogFile(const base::FilePath& dir,
                              size_t web_app_id,
                              const base::FilePath::StringPieceType& extension,
                              base::Time capture_time,
                              base::FilePath* file_path,
                              base::File* file) {
  *file_path =
      dir.AsEndingWithSeparator()
          .InsertBeforeExtensionASCII(kRemoteBoundWebRtcEventLogFileNamePrefix)
          .InsertBeforeExtensionASCII("_")
          .InsertBeforeExtensionASCII(base::NumberToString(web_app_id))
          .InsertBeforeExtensionASCII("_")
          .InsertBeforeExtensionASCII(CreateWebRtcEventLogId())
          .AddExtension(extension);

  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  file->Initialize(*file_path, file_flags);
  if (!file->IsValid() || !file->created()) {
    return false;
  }

  if (!base::TouchFile(*file_path, capture_time, capture_time)) {
    return false;
  }

  return true;
}

// This implementation does not upload files, nor pretends to have finished an
// upload. Most importantly, it does not get rid of the locally-stored log file
// after finishing a simulated upload; this is useful because it keeps the file
// on disk, where unit tests may inspect it.
// This class enforces an expectation over the upload being cancelled or not.
class NullWebRtcEventLogUploader : public WebRtcEventLogUploader {
 public:
  NullWebRtcEventLogUploader(const WebRtcLogFileInfo& log_file,
                             UploadResultCallback callback,
                             bool cancellation_expected)
      : log_file_(log_file),
        callback_(std::move(callback)),
        cancellation_expected_(cancellation_expected),
        was_cancelled_(false) {}

  ~NullWebRtcEventLogUploader() override {
    EXPECT_EQ(was_cancelled_, cancellation_expected_);
  }

  const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const override {
    return log_file_;
  }

  void Cancel() override {
    EXPECT_TRUE(cancellation_expected_);
    was_cancelled_ = true;
    if (callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), log_file_.path, false));
    }
  }

  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    Factory(bool cancellation_expected,
            std::optional<size_t> expected_instance_count = std::nullopt)
        : cancellation_expected_(cancellation_expected),
          expected_instance_count_(expected_instance_count),
          instance_count_(0) {}

    ~Factory() override {
      if (expected_instance_count_.has_value()) {
        EXPECT_EQ(instance_count_, expected_instance_count_.value());
      }
    }

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback) override {
      if (expected_instance_count_.has_value()) {
        EXPECT_LE(++instance_count_, expected_instance_count_.value());
      }
      return std::make_unique<NullWebRtcEventLogUploader>(
          log_file, std::move(callback), cancellation_expected_);
    }

   private:
    const bool cancellation_expected_;
    const std::optional<size_t> expected_instance_count_;
    size_t instance_count_;
  };

 private:
  const WebRtcLogFileInfo log_file_;
  UploadResultCallback callback_;
  const bool cancellation_expected_;
  bool was_cancelled_;
};

class MockWebRtcLocalEventLogsObserver : public WebRtcLocalEventLogsObserver {
 public:
  ~MockWebRtcLocalEventLogsObserver() override = default;
  MOCK_METHOD2(OnLocalLogStarted,
               void(PeerConnectionKey, const base::FilePath&));
  MOCK_METHOD1(OnLocalLogStopped, void(PeerConnectionKey));
};

class MockWebRtcRemoteEventLogsObserver : public WebRtcRemoteEventLogsObserver {
 public:
  ~MockWebRtcRemoteEventLogsObserver() override = default;
  MOCK_METHOD3(OnRemoteLogStarted,
               void(PeerConnectionKey, const base::FilePath&, int));
  MOCK_METHOD1(OnRemoteLogStopped, void(PeerConnectionKey));
};

}  // namespace

class WebRtcEventLogManagerTestBase : public ::testing::Test {
 public:
  WebRtcEventLogManagerTestBase()
      : test_shared_url_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()),
        run_loop_(std::make_unique<base::RunLoop>()),
        uploader_run_loop_(std::make_unique<base::RunLoop>()),
        browser_context_(nullptr),
        browser_context_id_(GetBrowserContextId(browser_context_.get())) {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_url_loader_factory_);

    // Avoid proactive pruning; it has the potential to mess up tests, as well
    // as keep pendings tasks around with a dangling reference to the unit
    // under test. (Zero is a sentinel value for disabling proactive pruning.)
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogProactivePruningDelta, "0");

    EXPECT_TRUE(local_logs_base_dir_.CreateUniqueTempDir());
    local_logs_base_path_ = local_logs_base_dir_.GetPath().Append(
        FILE_PATH_LITERAL("local_event_logs"));

    EXPECT_TRUE(profiles_dir_.CreateUniqueTempDir());
  }

  WebRtcEventLogManagerTestBase(const WebRtcEventLogManagerTestBase&) = delete;
  WebRtcEventLogManagerTestBase& operator=(
      const WebRtcEventLogManagerTestBase&) = delete;

  ~WebRtcEventLogManagerTestBase() override {
    WaitForPendingTasks();

    base::RunLoop run_loop;
    event_log_manager_->ShutDownForTesting(run_loop.QuitClosure());
    run_loop.Run();

    // We do not want to satisfy any unsatisfied expectations by destroying
    // |rph_|, |browser_context_|, etc., at the end of the test, before we
    // destroy |event_log_manager_|. However, we must also make sure that their
    // destructors do not attempt to access |event_log_manager_|, which in
    // normal code lives forever, but not in the unit tests.
    event_log_manager_.reset();

    // Guard against unexpected state changes.
    EXPECT_TRUE(webrtc_state_change_instructions_.empty());

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
#endif
  }

  void SetUp() override {
    SetUpNetworkConnection(true,
                           network::mojom::ConnectionType::CONNECTION_ETHERNET);
    SetLocalLogsObserver(&local_observer_);
    SetRemoteLogsObserver(&remote_observer_);
    LoadMainTestProfile();
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
#endif
  }

  void SetUpNetworkConnection(bool respond_synchronously,
                              network::mojom::ConnectionType connection_type) {
    auto* tracker = network::TestNetworkConnectionTracker::GetInstance();
    tracker->SetRespondSynchronously(respond_synchronously);
    tracker->SetConnectionType(connection_type);
  }

  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  void CreateWebRtcEventLogManager(
      std::optional<Compression> remote = std::nullopt) {
    DCHECK(!event_log_manager_);

    event_log_manager_ = WebRtcEventLogManager::CreateSingletonInstance();

    local_log_extension_ = kWebRtcEventLogUncompressedExtension;

    if (remote.has_value()) {
      auto factory = CreateLogFileWriterFactory(remote.value());
      remote_log_extension_ = factory->Extension();
      event_log_manager_->SetRemoteLogFileWriterFactoryForTesting(
          std::move(factory));
    } else {
      // kWebRtcRemoteEventLogGzipped is turned on by default.
      remote_log_extension_ = kWebRtcEventLogGzippedExtension;
    }
  }

  void LoadMainTestProfile() {
    browser_context_ = CreateBrowserContext("browser_context_");
    browser_context_id_ = GetBrowserContextId(browser_context_.get());
    rph_ = std::make_unique<MockRenderProcessHost>(browser_context_.get());
  }

  void UnloadMainTestProfile() {
    rph_.reset();
    browser_context_.reset();
    browser_context_id_ = GetBrowserContextId(browser_context_.get());
  }

  void WaitForReply() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();  // Allow re-blocking.
  }

  void Reply() { run_loop_->QuitWhenIdle(); }

  base::OnceClosure ReplyClosure() {
    // Intermediary pointer used to help the compiler distinguish between
    // the overloaded Reply() functions.
    void (WebRtcEventLogManagerTestBase::*function)() =
        &WebRtcEventLogManagerTestBase::Reply;
    return base::BindOnce(function, base::Unretained(this));
  }

  template <typename T>
  void Reply(T* output, T val) {
    *output = val;
    run_loop_->QuitWhenIdle();
  }

  template <typename T>
  base::OnceCallback<void(T)> ReplyClosure(T* output) {
    // Intermediary pointer used to help the compiler distinguish between
    // the overloaded Reply() functions.
    void (WebRtcEventLogManagerTestBase::*function)(T*, T) =
        &WebRtcEventLogManagerTestBase::Reply;
    return base::BindOnce(function, base::Unretained(this), output);
  }

  void Reply(bool* output_bool,
             std::string* output_str1,
             std::string* output_str2,
             bool bool_val,
             const std::string& str1_val,
             const std::string& str2_val) {
    *output_bool = bool_val;
    *output_str1 = str1_val;
    *output_str2 = str2_val;
    run_loop_->QuitWhenIdle();
  }

  base::OnceCallback<void(bool, const std::string&, const std::string&)>
  ReplyClosure(bool* output_bool,
               std::string* output_str1,
               std::string* output_str2) {
    // Intermediary pointer used to help the compiler distinguish between
    // the overloaded Reply() functions.
    void (WebRtcEventLogManagerTestBase::*function)(
        bool*, std::string*, std::string*, bool, const std::string&,
        const std::string&) = &WebRtcEventLogManagerTestBase::Reply;
    return base::BindOnce(function, base::Unretained(this), output_bool,
                          output_str1, output_str2);
  }

  bool OnPeerConnectionAdded(const PeerConnectionKey& key) {
    bool result;
    event_log_manager_->OnPeerConnectionAdded(
        content::GlobalRenderFrameHostId(key.render_process_id,
                                         key.render_frame_id),
        key.lid, ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool OnPeerConnectionRemoved(const PeerConnectionKey& key) {
    bool result;
    event_log_manager_->OnPeerConnectionRemoved(
        content::GlobalRenderFrameHostId(key.render_process_id,
                                         key.render_frame_id),
        key.lid, ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool OnPeerConnectionSessionIdSet(const PeerConnectionKey& key,
                                    const std::string& session_id) {
    bool result;
    event_log_manager_->OnPeerConnectionSessionIdSet(
        content::GlobalRenderFrameHostId(key.render_process_id,
                                         key.render_frame_id),
        key.lid, session_id, ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool OnPeerConnectionSessionIdSet(const PeerConnectionKey& key) {
    return OnPeerConnectionSessionIdSet(key, GetUniqueId(key));
  }

  bool OnPeerConnectionStopped(const PeerConnectionKey& key) {
    bool result;
    event_log_manager_->OnPeerConnectionStopped(
        content::GlobalRenderFrameHostId(key.render_process_id,
                                         key.render_frame_id),
        key.lid, ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool EnableLocalLogging(
      size_t max_size_bytes = kWebRtcEventLogManagerUnlimitedFileSize) {
    return EnableLocalLogging(local_logs_base_path_, max_size_bytes);
  }

  bool EnableLocalLogging(
      base::FilePath local_logs_base_path,
      size_t max_size_bytes = kWebRtcEventLogManagerUnlimitedFileSize) {
    bool result;
    event_log_manager_->EnableLocalLogging(local_logs_base_path, max_size_bytes,
                                           ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool DisableLocalLogging() {
    bool result;
    event_log_manager_->DisableLocalLogging(ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  bool StartRemoteLogging(const PeerConnectionKey& key,
                          const std::string& session_id,
                          size_t max_size_bytes,
                          int output_period_ms,
                          size_t web_app_id,
                          std::string* log_id_output = nullptr,
                          std::string* error_message_output = nullptr) {
    bool result;
    std::string log_id;
    std::string error_message;

    event_log_manager_->StartRemoteLogging(
        key.render_process_id, session_id, max_size_bytes, output_period_ms,
        web_app_id, ReplyClosure(&result, &log_id, &error_message));

    WaitForReply();

    // If successful, only |log_id|. If unsuccessful, only |error_message| set.
    DCHECK_EQ(result, !log_id.empty());
    DCHECK_EQ(!result, !error_message.empty());

    if (log_id_output) {
      *log_id_output = log_id;
    }

    if (error_message_output) {
      *error_message_output = error_message;
    }

    return result;
  }

  bool StartRemoteLogging(const PeerConnectionKey& key,
                          const std::string& session_id,
                          std::string* log_id_output = nullptr,
                          std::string* error_message_output = nullptr) {
    return StartRemoteLogging(key, session_id, kMaxRemoteLogFileSizeBytes, 0,
                              kWebAppId, log_id_output, error_message_output);
  }

  bool StartRemoteLogging(const PeerConnectionKey& key,
                          std::string* log_id_output = nullptr,
                          std::string* error_message_output = nullptr) {
    return StartRemoteLogging(key, GetUniqueId(key), kMaxRemoteLogFileSizeBytes,
                              0, kWebAppId, log_id_output,
                              error_message_output);
  }

  void ClearCacheForBrowserContext(
      const content::BrowserContext* browser_context,
      const base::Time& delete_begin,
      const base::Time& delete_end) {
    event_log_manager_->ClearCacheForBrowserContext(
        browser_context, delete_begin, delete_end, ReplyClosure());
    WaitForReply();
  }

  std::vector<UploadList::UploadInfo> GetHistory(
      BrowserContextId browser_context_id) {
    std::vector<UploadList::UploadInfo> result;

    base::RunLoop run_loop;

    auto reply = [](base::RunLoop* run_loop,
                    std::vector<UploadList::UploadInfo>* output,
                    const std::vector<UploadList::UploadInfo>& input) {
      *output = input;
      run_loop->Quit();
    };
    event_log_manager_->GetHistory(browser_context_id,
                                   base::BindOnce(reply, &run_loop, &result));
    run_loop.Run();

    return result;
  }

  void SetLocalLogsObserver(WebRtcLocalEventLogsObserver* observer) {
    event_log_manager_->SetLocalLogsObserver(observer, ReplyClosure());
    WaitForReply();
  }

  void SetRemoteLogsObserver(WebRtcRemoteEventLogsObserver* observer) {
    event_log_manager_->SetRemoteLogsObserver(observer, ReplyClosure());
    WaitForReply();
  }

  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> factory) {
    event_log_manager_->SetWebRtcEventLogUploaderFactoryForTesting(
        std::move(factory), ReplyClosure());
    WaitForReply();
  }

  std::pair<bool, bool> OnWebRtcEventLogWrite(const PeerConnectionKey& key,
                                              const std::string& message) {
    std::pair<bool, bool> result;
    event_log_manager_->OnWebRtcEventLogWrite(
        content::GlobalRenderFrameHostId(key.render_process_id,
                                         key.render_frame_id),
        key.lid, message, ReplyClosure(&result));
    WaitForReply();
    return result;
  }

  void FreezeClockAt(const base::Time::Exploded& frozen_time_exploded) {
    base::Time frozen_time;
    ASSERT_TRUE(
        base::Time::FromLocalExploded(frozen_time_exploded, &frozen_time));
    frozen_clock_.SetNow(frozen_time);
    event_log_manager_->SetClockForTesting(&frozen_clock_, ReplyClosure());
    WaitForReply();
  }

  void SetWebRtcEventLoggingState(const PeerConnectionKey& key,
                                  bool event_logging_enabled) {
    webrtc_state_change_instructions_.emplace(key, event_logging_enabled);
  }

  void ExpectWebRtcStateChangeInstruction(const PeerConnectionKey& key,
                                          bool enabled) {
    ASSERT_FALSE(webrtc_state_change_instructions_.empty());
    auto& instruction = webrtc_state_change_instructions_.front();
    EXPECT_EQ(instruction.key.render_process_id, key.render_process_id);
    EXPECT_EQ(instruction.key.lid, key.lid);
    EXPECT_EQ(instruction.enabled, enabled);
    webrtc_state_change_instructions_.pop();
  }

  void SetPeerConnectionTrackerProxyForTesting(
      std::unique_ptr<WebRtcEventLogManager::PeerConnectionTrackerProxy>
          pc_tracker_proxy) {
    event_log_manager_->SetPeerConnectionTrackerProxyForTesting(
        std::move(pc_tracker_proxy), ReplyClosure());
    WaitForReply();
  }

  // Allows either creating a TestingProfile with a predetermined name
  // (useful when trying to "reload" a profile), or one with an arbitrary name.
  virtual std::unique_ptr<TestingProfile> CreateBrowserContext() {
    return CreateBrowserContext(std::string());
  }
  virtual std::unique_ptr<TestingProfile> CreateBrowserContext(
      std::string profile_name) {
    return CreateBrowserContext(profile_name, true /* is_managed_profile */,
                                false /* has_device_level_policies */,
                                true /* policy_allows_remote_logging */);
  }
  virtual std::unique_ptr<TestingProfile> CreateBrowserContext(
      std::string profile_name,
      bool is_managed_profile,
      bool has_device_level_policies,
      std::optional<bool> policy_allows_remote_logging) {
    return CreateBrowserContextWithCustomSupervision(
        profile_name, is_managed_profile, has_device_level_policies,
        false /* is_supervised */, policy_allows_remote_logging);
  }
  virtual std::unique_ptr<TestingProfile>
  CreateBrowserContextWithCustomSupervision(
      std::string profile_name,
      bool is_managed_profile,
      bool has_device_level_policies,
      bool is_supervised,
      std::optional<bool> policy_allows_remote_logging) {
    // If profile name not specified, select a unique name.
    if (profile_name.empty()) {
      static size_t index = 0;
      profile_name = base::NumberToString(++index);
    }

    // Set a directory for the profile, derived from its name, so that
    // recreating the profile will get the same directory.
    const base::FilePath profile_path =
        profiles_dir_.GetPath().AppendASCII(profile_name);
    if (base::PathExists(profile_path)) {
      EXPECT_TRUE(base::DirectoryExists(profile_path));
    } else {
      EXPECT_TRUE(base::CreateDirectory(profile_path));
    }

    // Prepare to specify preferences for the profile.
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_user_prefs(base::WrapRefCounted(new TestingPrefStore()));
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    sync_preferences::PrefServiceSyncable* regular_prefs =
        factory.CreateSyncable(registry.get()).release();

    // Set the preference associated with the policy for WebRTC remote-bound
    // event logging.
    RegisterUserProfilePrefs(registry.get());
    if (policy_allows_remote_logging.has_value()) {
      regular_prefs->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                policy_allows_remote_logging.value());
    }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    policy::PolicyMap policy_map;
    if (has_device_level_policies) {
      policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                     policy::POLICY_SCOPE_MACHINE,
                     policy::POLICY_SOURCE_PLATFORM, base::Value("test"),
                     nullptr);
    }
    provider_.UpdateChromePolicy(policy_map);
#else
    if (has_device_level_policies) {
      ADD_FAILURE() << "Invalid test setup. Chrome platform policies cannot be "
                       "set on Chrome OS and Android.";
    }
#endif

    // Build the profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(profile_name);
    profile_builder.SetPath(profile_path);
    profile_builder.SetPrefService(base::WrapUnique(regular_prefs));
    profile_builder.OverridePolicyConnectorIsManagedForTesting(
        is_managed_profile);
    if (is_supervised) {
      profile_builder.SetIsSupervisedProfile();
    }
    std::unique_ptr<TestingProfile> profile = profile_builder.Build();

    // Blocks on the unit under test's task runner, so that we won't proceed
    // with the test (e.g. check that files were created) before finished
    // processing this even (which is signaled to it from
    //  BrowserContext::EnableForBrowserContext).
    WaitForPendingTasks();

    return profile;
  }

  base::FilePath RemoteBoundLogsDir(BrowserContext* browser_context) const {
    return RemoteBoundLogsDir(browser_context->GetPath());
  }

  base::FilePath RemoteBoundLogsDir(
      const base::FilePath& browser_context_base_dir) const {
    return GetRemoteBoundWebRtcEventLogsDir(browser_context_base_dir);
  }

  // Initiate an arbitrary synchronous operation, allowing any tasks pending
  // on the manager's internal task queue to be completed.
  // If given a RunLoop, we first block on it. The reason to do both is that
  // with the RunLoop we wait on some tasks which we know also post additional
  // tasks, then, after that chain is completed, we also wait for any potential
  // leftovers. For example, the run loop could wait for n-1 files to be
  // uploaded, then it is released when the last one's upload is initiated,
  // then we wait for the last file's upload to be completed.
  void WaitForPendingTasks(base::RunLoop* run_loop = nullptr) {
    if (run_loop) {
      run_loop->Run();
    }

    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    event_log_manager_->GetTaskRunnerForTesting()->PostTask(
        FROM_HERE,
        base::BindOnce([](base::WaitableEvent* event) { event->Signal(); },
                       &event));
    event.Wait();
  }

  void SuppressUploading() {
    if (!upload_suppressing_browser_context_) {  // First suppression.
      upload_suppressing_browser_context_ = CreateBrowserContext();
    }
    DCHECK(!upload_suppressing_rph_) << "Uploading already suppressed.";
    upload_suppressing_rph_ = std::make_unique<MockRenderProcessHost>(
        upload_suppressing_browser_context_.get());
    const auto key = GetPeerConnectionKey(upload_suppressing_rph_.get(), 0);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
  }

  void UnsuppressUploading() {
    DCHECK(upload_suppressing_rph_) << "Uploading not suppressed.";
    const auto key = GetPeerConnectionKey(upload_suppressing_rph_.get(), 0);
    ASSERT_TRUE(OnPeerConnectionRemoved(key));
    upload_suppressing_rph_.reset();
  }

  void ExpectLocalFileContents(const base::FilePath& file_path,
                               const std::string& expected_contents) {
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
    EXPECT_EQ(file_contents, expected_contents);
  }

  void ExpectRemoteFileContents(const base::FilePath& file_path,
                                const std::string& expected_event_log) {
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));

    if (remote_log_extension_ == kWebRtcEventLogUncompressedExtension) {
      EXPECT_EQ(file_contents, expected_event_log);
    } else if (remote_log_extension_ == kWebRtcEventLogGzippedExtension) {
      std::string uncompressed_log;
      ASSERT_TRUE(
          compression::GzipUncompress(file_contents, &uncompressed_log));
      EXPECT_EQ(uncompressed_log, expected_event_log);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  // When the peer connection's ID is not the focus of the test, this allows
  // us to conveniently assign unique IDs to peer connections.
  std::string GetUniqueId(int render_process_id, int lid) {
    return base::NumberToString(render_process_id) + "_" +
           base::NumberToString(lid);
  }
  std::string GetUniqueId(const PeerConnectionKey& key) {
    return GetUniqueId(key.render_process_id, key.lid);
  }

  bool UploadConditionsHold() {
    base::RunLoop run_loop;
    bool result;

    auto callback = [](base::RunLoop* run_loop, bool* result_out, bool result) {
      *result_out = result;
      run_loop->QuitWhenIdle();
    };

    event_log_manager_->UploadConditionsHoldForTesting(
        base::BindOnce(callback, &run_loop, &result));
    run_loop.Run();

    return result;
  }

  // Testing utilities.
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  base::SimpleTestClock frozen_clock_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::MockConfigurationPolicyProvider provider_;
#endif

  // The main loop, which allows waiting for the operations invoked on the
  // unit-under-test to be completed. Do not use this object directly from the
  // tests, since that would be error-prone. (Specifically, one must not produce
  // two events that could produce replies, without waiting on the first reply
  // in between.)
  std::unique_ptr<base::RunLoop> run_loop_;

  // Allows waiting for upload operations.
  std::unique_ptr<base::RunLoop> uploader_run_loop_;

  // Unit under test.
  std::unique_ptr<WebRtcEventLogManager> event_log_manager_;

  // Extensions associated with local/remote-bound event logs. Depends on
  // whether they're compressed.
  base::FilePath::StringPieceType local_log_extension_;
  base::FilePath::StringPieceType remote_log_extension_;

  // The directory which will contain all profiles.
  base::ScopedTempDir profiles_dir_;

  // Default BrowserContext and RenderProcessHost, to be used by tests which
  // do not require anything fancy (such as seeding the BrowserContext with
  // pre-existing logs files from a previous session, or working with multiple
  // BrowserContext objects).

  std::unique_ptr<TestingProfile> browser_context_;
  BrowserContextId browser_context_id_;
  std::unique_ptr<MockRenderProcessHost> rph_;

  // Used for suppressing the upload of finished files, by creating an active
  // remote-bound log associated with an independent BrowserContext which
  // does not otherwise interfere with the test.
  std::unique_ptr<TestingProfile> upload_suppressing_browser_context_;
  std::unique_ptr<MockRenderProcessHost> upload_suppressing_rph_;

  // The directory where we'll save local log files.
  base::ScopedTempDir local_logs_base_dir_;
  // local_logs_base_dir_ +  log files' name prefix.
  base::FilePath local_logs_base_path_;

  // WebRtcEventLogManager instructs WebRTC, via PeerConnectionTracker, to
  // only send WebRTC messages for certain peer connections. Some tests make
  // sure that this is done correctly, by waiting for these notifications, then
  // testing them.
  // Because a single action - disabling of local logging - could crease a
  // series of such instructions, we keep a queue of them. However, were one
  // to actually test that scenario, one would have to account for the lack
  // of a guarantee over the order in which these instructions are produced.
  struct WebRtcStateChangeInstruction {
    WebRtcStateChangeInstruction(PeerConnectionKey key, bool enabled)
        : key(key), enabled(enabled) {}
    PeerConnectionKey key;
    bool enabled;
  };
  std::queue<WebRtcStateChangeInstruction> webrtc_state_change_instructions_;

  // Observers for local/remote logging being started/stopped. By having them
  // here, we achieve two goals:
  // 1. Reduce boilerplate in the tests themselves.
  // 2. Avoid lifetime issues, where the observer might be deallocated before
  //    a RenderProcessHost is deallocated (which can potentially trigger a
  //    callback on the observer).
  NiceMock<MockWebRtcLocalEventLogsObserver> local_observer_;
  NiceMock<MockWebRtcRemoteEventLogsObserver> remote_observer_;
};

#if !BUILDFLAG(IS_ANDROID)

class WebRtcEventLogManagerTest : public WebRtcEventLogManagerTestBase,
                                  public ::testing::WithParamInterface<bool> {
 public:
  WebRtcEventLogManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    // Use a low delay, or the tests would run for quite a long time.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "100");
  }

  ~WebRtcEventLogManagerTest() override = default;

  void SetUp() override {
    CreateWebRtcEventLogManager(Compression::GZIP_PERFECT_ESTIMATION);

    WebRtcEventLogManagerTestBase::SetUp();

    SetWebRtcEventLogUploaderFactoryForTesting(
        std::make_unique<NullWebRtcEventLogUploader::Factory>(false));
  }
};

class WebRtcEventLogManagerTestCacheClearing
    : public WebRtcEventLogManagerTest {
 public:
  ~WebRtcEventLogManagerTestCacheClearing() override = default;

  void CreatePendingLogFiles(BrowserContext* browser_context) {
    ASSERT_TRUE(pending_logs_.find(browser_context) == pending_logs_.end());
    auto& elements = pending_logs_[browser_context];
    elements = std::make_unique<BrowserContextAssociatedElements>();

    for (size_t i = 0; i < kMaxActiveRemoteBoundWebRtcEventLogs; ++i) {
      elements->rphs.push_back(
          std::make_unique<MockRenderProcessHost>(browser_context));
      const auto key = GetPeerConnectionKey(elements->rphs[i].get(), kLid);
      elements->file_paths.push_back(CreatePendingRemoteLogFile(key));
      ASSERT_TRUE(elements->file_paths[i]);
      ASSERT_TRUE(base::PathExists(*elements->file_paths[i]));

      pending_latest_mod_ = GetLastModificationTime(*elements->file_paths[i]);
      if (pending_earliest_mod_.is_null()) {  // First file.
        pending_earliest_mod_ = pending_latest_mod_;
      }
    }
  }

  void ClearPendingLogFiles() { pending_logs_.clear(); }

  std::optional<base::FilePath> CreateRemoteLogFile(
      const PeerConnectionKey& key,
      bool pending) {
    std::optional<base::FilePath> file_path;
    ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
    EXPECT_TRUE(OnPeerConnectionAdded(key));
    EXPECT_TRUE(OnPeerConnectionSessionIdSet(key));
    EXPECT_TRUE(StartRemoteLogging(key));
    if (pending) {
      // Transition from ACTIVE to PENDING.
      EXPECT_TRUE(OnPeerConnectionRemoved(key));
    }
    return file_path;
  }

  std::optional<base::FilePath> CreateActiveRemoteLogFile(
      const PeerConnectionKey& key) {
    return CreateRemoteLogFile(key, false);
  }

  std::optional<base::FilePath> CreatePendingRemoteLogFile(
      const PeerConnectionKey& key) {
    return CreateRemoteLogFile(key, true);
  }

 protected:
  // When closing a file, rather than check its last modification date, which
  // is potentially expensive, WebRtcRemoteEventLogManager reads the system
  // clock, which should be close enough. For tests, however, the difference
  // could be enough to flake the tests, if not for this epsilon. Given the
  // focus of the tests that use this, this epsilon can be arbitrarily large.
  static const base::TimeDelta kEpsion;

  struct BrowserContextAssociatedElements {
    std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
    std::vector<std::optional<base::FilePath>> file_paths;
  };

  std::map<const BrowserContext*,
           std::unique_ptr<BrowserContextAssociatedElements>>
      pending_logs_;

  // Latest modification times of earliest and latest pending log files.
  base::Time pending_earliest_mod_;
  base::Time pending_latest_mod_;
};

const base::TimeDelta WebRtcEventLogManagerTestCacheClearing::kEpsion =
    base::Hours(1);

class WebRtcEventLogManagerTestWithRemoteLoggingDisabled
    : public WebRtcEventLogManagerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  WebRtcEventLogManagerTestWithRemoteLoggingDisabled()
      : feature_enabled_(GetParam()), policy_enabled_(!feature_enabled_) {
    if (feature_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kWebRtcRemoteEventLog);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebRtcRemoteEventLog);
    }
    CreateWebRtcEventLogManager();
  }

  ~WebRtcEventLogManagerTestWithRemoteLoggingDisabled() override = default;

  // Override CreateBrowserContext() to use policy_enabled_.
  std::unique_ptr<TestingProfile> CreateBrowserContext() override {
    return CreateBrowserContext(std::string());
  }
  std::unique_ptr<TestingProfile> CreateBrowserContext(
      std::string profile_name) override {
    return CreateBrowserContext(profile_name, policy_enabled_,
                                false /* has_device_level_policies */,
                                policy_enabled_);
  }
  std::unique_ptr<TestingProfile> CreateBrowserContext(
      std::string profile_name,
      bool is_managed_profile,
      bool has_device_level_policies,
      std::optional<bool> policy_allows_remote_logging) override {
    DCHECK_EQ(policy_enabled_, policy_allows_remote_logging.value());
    return WebRtcEventLogManagerTestBase::CreateBrowserContext(
        profile_name, is_managed_profile, has_device_level_policies,
        policy_allows_remote_logging);
  }

 private:
  const bool feature_enabled_;  // Whether the Finch kill-switch is engaged.
  const bool policy_enabled_;  // Whether the policy is enabled for the profile.
};

class WebRtcEventLogManagerTestPolicy : public WebRtcEventLogManagerTestBase {
 public:
  ~WebRtcEventLogManagerTestPolicy() override = default;

  // Defer to setup from the body.
  void SetUp() override {}

  void SetUp(bool feature_enabled) {
    if (feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kWebRtcRemoteEventLog);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebRtcRemoteEventLog);
    }

    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "0");

    CreateWebRtcEventLogManager(Compression::GZIP_PERFECT_ESTIMATION);

    WebRtcEventLogManagerTestBase::SetUp();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> GetScopedUserManager(
      user_manager::UserType user_type);
#endif

  void TestManagedProfileAfterBeingExplicitlySet(bool explicitly_set_value);
};

class WebRtcEventLogManagerTestUploadSuppressionDisablingFlag
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestUploadSuppressionDisablingFlag() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ::switches::kWebRtcRemoteEventLogUploadNoSuppression);

    // Use a low delay, or the tests would run for quite a long time.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "100");

    CreateWebRtcEventLogManager();
  }

  ~WebRtcEventLogManagerTestUploadSuppressionDisablingFlag() override = default;
};

class WebRtcEventLogManagerTestForNetworkConnectivity
    : public WebRtcEventLogManagerTestBase,
      public ::testing::WithParamInterface<
          std::tuple<bool,
                     network::mojom::ConnectionType,
                     network::mojom::ConnectionType>> {
 public:
  WebRtcEventLogManagerTestForNetworkConnectivity()
      : get_conn_type_is_sync_(std::get<0>(GetParam())),
        supported_type_(std::get<1>(GetParam())),
        unsupported_type_(std::get<2>(GetParam())) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    // Use a low delay, or the tests would run for quite a long time.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "100");

    CreateWebRtcEventLogManager();
  }

  ~WebRtcEventLogManagerTestForNetworkConnectivity() override = default;

  void UnloadProfileAndSeedPendingLog() {
    DCHECK(browser_context_path_.empty()) << "Not expected to be called twice.";

    // Unload the profile, but remember where it stores its files (for sanity).
    browser_context_path_ = browser_context_->GetPath();
    const base::FilePath remote_logs_dir =
        RemoteBoundLogsDir(browser_context_.get());
    UnloadMainTestProfile();

    // Seed the remote logs' directory with one log file, simulating the
    // creation of logs in a previous session.
    ASSERT_TRUE(base::CreateDirectory(remote_logs_dir));

    base::FilePath file_path;
    ASSERT_TRUE(CreateRemoteBoundLogFile(
        remote_logs_dir, kWebAppId, remote_log_extension_, base::Time::Now(),
        &file_path, &file_));

    expected_files_.emplace_back(browser_context_id_, file_path,
                                 GetLastModificationTime(file_path));
  }

  const bool get_conn_type_is_sync_;
  const network::mojom::ConnectionType supported_type_;
  const network::mojom::ConnectionType unsupported_type_;

  base::FilePath browser_context_path_;  // For sanity over the test itself.
  std::list<WebRtcLogFileInfo> expected_files_;
  base::File file_;
};

class WebRtcEventLogManagerTestUploadDelay
    : public WebRtcEventLogManagerTestBase {
 public:
  ~WebRtcEventLogManagerTestUploadDelay() override = default;

  void SetUp() override {
    // Intercept and block the call to SetUp(). The test body will call
    // the version that sets an upload delay instead.
  }

  void SetUp(size_t upload_delay_ms) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs,
        base::NumberToString(upload_delay_ms));

    CreateWebRtcEventLogManager();

    WebRtcEventLogManagerTestBase::SetUp();
  }

  // There's a trade-off between the test runtime and the likelihood of a
  // false-positive (lowered when the time is increased).
  // Since false-positives can be caught handled even if only manifesting
  // occasionally, this value should be enough.
  static const size_t kDefaultUploadDelayMs = 500;

  // For tests where we don't intend to wait, prevent flakiness by setting
  // an unrealistically long delay.
  static const size_t kIntentionallyExcessiveDelayMs = 1000 * 1000 * 1000;
};

// For testing compression issues.
class WebRtcEventLogManagerTestCompression
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestCompression() {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);

    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "0");
  }

  ~WebRtcEventLogManagerTestCompression() override = default;

  void SetUp() override {
    // Defer until Init(), which will allow the test body more control.
  }

  void Init(std::optional<WebRtcEventLogCompression> remote_compression =
                std::optional<WebRtcEventLogCompression>()) {
    CreateWebRtcEventLogManager(remote_compression);

    WebRtcEventLogManagerTestBase::SetUp();
  }
};

class WebRtcEventLogManagerTestIncognito
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestIncognito() : incognito_profile_(nullptr) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebRtcRemoteEventLog);
    CreateWebRtcEventLogManager();
  }

  ~WebRtcEventLogManagerTestIncognito() override {
    incognito_rph_.reset();
    if (incognito_profile_) {
      DCHECK(browser_context_);
      browser_context_->DestroyOffTheRecordProfile(incognito_profile_);
    }
  }

  void SetUp() override {
    WebRtcEventLogManagerTestBase::SetUp();

    incognito_profile_ =
        browser_context_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    incognito_rph_ =
        std::make_unique<MockRenderProcessHost>(incognito_profile_);
  }

  raw_ptr<Profile, DanglingUntriaged> incognito_profile_;
  std::unique_ptr<MockRenderProcessHost> incognito_rph_;
};

class WebRtcEventLogManagerTestHistory : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestHistory() {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kWebRtcRemoteEventLogUploadDelayMs, "0");

    CreateWebRtcEventLogManager();
  }

  ~WebRtcEventLogManagerTestHistory() override = default;

  // Allows us to test that a time is as expected, down to UNIX time's
  // lower resolution than our clock.
  static bool IsSameTimeWhenTruncatedToSeconds(base::Time a, base::Time b) {
    if (a.is_null() || b.is_null()) {
      return false;
    }
    const base::TimeDelta delta = std::max(a, b) - std::min(a, b);
    return delta.InSeconds() == 0;
  }

  // Allows us to check that the timestamps are roughly what we expect.
  // Doing better than this would require too much effort.
  static bool IsSmallTimeDelta(base::Time a, base::Time b) {
    if (a.is_null() || b.is_null()) {
      return false;
    }

    // Way more than is "small", to make sure tests don't become flaky.
    // If the timestamp is ever off, it's likely to be off by more than this,
    // though, or the problem would not truly be severe enough to worry about.
    const base::TimeDelta small_delta = base::Minutes(15);

    return (std::max(a, b) - std::min(a, b) <= small_delta);
  }
};

namespace {

class PeerConnectionTrackerProxyForTesting
    : public WebRtcEventLogManager::PeerConnectionTrackerProxy {
 public:
  explicit PeerConnectionTrackerProxyForTesting(
      WebRtcEventLogManagerTestBase* test)
      : test_(test) {}

  ~PeerConnectionTrackerProxyForTesting() override = default;

  void EnableWebRtcEventLogging(const PeerConnectionKey& key,
                                int output_period_ms) override {
    test_->SetWebRtcEventLoggingState(key, true);
  }
  void DisableWebRtcEventLogging(const PeerConnectionKey& key) override {
    test_->SetWebRtcEventLoggingState(key, false);
  }

 private:
  const raw_ptr<WebRtcEventLogManagerTestBase> test_;
};

// The factory for the following fake uploader produces a sequence of
// uploaders which fail the test if given a file other than that which they
// expect. The factory itself likewise fails the test if destroyed before
// producing all expected uploaders, or if it's asked for more uploaders than
// it expects to create. This allows us to test for sequences of uploads.
class FileListExpectingWebRtcEventLogUploader : public WebRtcEventLogUploader {
 public:
  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    Factory(std::list<WebRtcLogFileInfo>* expected_files,
            bool result,
            base::RunLoop* run_loop)
        : result_(result), run_loop_(run_loop) {
      expected_files_.swap(*expected_files);
      if (expected_files_.empty()) {
        run_loop_->QuitWhenIdle();
      }
    }

    ~Factory() override { EXPECT_TRUE(expected_files_.empty()); }

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback) override {
      if (expected_files_.empty()) {
        EXPECT_FALSE(true);  // More files uploaded than expected.
      } else {
        EXPECT_EQ(log_file.path, expected_files_.front().path);
        // Because LoadMainTestProfile() and UnloadMainTestProfile() mess up the
        // BrowserContextId in ways that would not happen in production,
        // we cannot verify |log_file.browser_context_id| is correct.
        // This is unimportant to the test.

        base::DeleteFile(log_file.path);
        expected_files_.pop_front();
      }

      if (expected_files_.empty()) {
        run_loop_->QuitWhenIdle();
      }

      return std::make_unique<FileListExpectingWebRtcEventLogUploader>(
          log_file, result_, std::move(callback));
    }

   private:
    std::list<WebRtcLogFileInfo> expected_files_;
    const bool result_;
    const raw_ptr<base::RunLoop> run_loop_;
  };

  // The logic is in the factory; the uploader just reports success so that the
  // next file may become eligible for uploading.
  FileListExpectingWebRtcEventLogUploader(const WebRtcLogFileInfo& log_file,
                                          bool result,
                                          UploadResultCallback callback)
      : log_file_(log_file) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), log_file_.path, result));
  }

  ~FileListExpectingWebRtcEventLogUploader() override = default;

  const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const override {
    return log_file_;
  }

  void Cancel() override {
    NOTREACHED_IN_MIGRATION() << "Incompatible with this kind of test.";
  }

 private:
  const WebRtcLogFileInfo log_file_;
};

}  // namespace

TEST_F(WebRtcEventLogManagerTest, OnPeerConnectionAddedReturnsTrue) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_TRUE(OnPeerConnectionAdded(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionAddedReturnsFalseIfAlreadyAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_FALSE(OnPeerConnectionAdded(key));
}

TEST_F(WebRtcEventLogManagerTest, OnPeerConnectionRemovedReturnsTrue) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_TRUE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionRemovedReturnsFalseIfNeverAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_FALSE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionRemovedReturnsFalseIfAlreadyRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  EXPECT_FALSE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest, OnPeerConnectionSessionIdSetReturnsTrue) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_TRUE(OnPeerConnectionSessionIdSet(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionSessionIdSetReturnsFalseIfEmptyString) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_FALSE(OnPeerConnectionSessionIdSet(key, ""));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionSessionIdSetReturnsFalseIfPeerConnectionNeverAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_FALSE(OnPeerConnectionSessionIdSet(key, kSessionId));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionSessionIdSetReturnsFalseIfPeerConnectionAlreadyRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  EXPECT_FALSE(OnPeerConnectionSessionIdSet(key, kSessionId));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionSessionIdSetReturnsTrueIfAlreadyCalledSameId) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  EXPECT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionSessionIdSetReturnsFalseIfAlreadyCalledDifferentId) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, "id1"));
  EXPECT_FALSE(OnPeerConnectionSessionIdSet(key, "id2"));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionSessionIdSetCalledOnRecreatedPeerConnectionSanity) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
}

TEST_F(WebRtcEventLogManagerTest, EnableLocalLoggingReturnsTrue) {
  EXPECT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       EnableLocalLoggingReturnsFalseIfCalledWhenAlreadyEnabled) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_FALSE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, DisableLocalLoggingReturnsTrue) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, DisableLocalLoggingReturnsIfNeverEnabled) {
  EXPECT_FALSE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest, DisableLocalLoggingReturnsIfAlreadyDisabled) {
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_FALSE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsFalseAndFalseWhenAllLoggingDisabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  // Note that EnableLocalLogging() and StartRemoteLogging() weren't called.
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsFalseAndFalseForUnknownPeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(EnableLocalLogging());
  // Note that OnPeerConnectionAdded() wasn't called.
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsLocalTrueWhenPcKnownAndLocalLoggingOn) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(true, false));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsRemoteTrueWhenPcKnownAndRemoteLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(false, true));
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteReturnsTrueAndTrueeWhenAllLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key));
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(true, true));
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedNotCalledIfLocalLoggingEnabledWithoutPeerConnections) {
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedNotCalledIfLocalLoggingDisabledWithoutPeerConnections) {
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedCalledForOnPeerConnectionAddedAndLocalLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStartedCalledForLocalLoggingEnabledAndOnPeerConnectionAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedCalledAfterLocalLoggingDisabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(DisableLocalLogging());
}

TEST_F(WebRtcEventLogManagerTest,
       OnLocalLogStoppedCalledAfterOnPeerConnectionRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreatesEmptyFileWhenStarted) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(*file_path, std::string());
}

TEST_F(WebRtcEventLogManagerTest, LocalLogCreateAndWriteToFile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log = "To strive, to seek, to find, and not to yield.";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(*file_path, log);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleWritesToSameFile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string logs[] = {"Old age hath yet his honour and his toil;",
                              "Death closes all: but something ere the end,",
                              "Some work of noble note, may yet be done,",
                              "Not unbecoming men that strove with Gods."};
  for (const std::string& log : logs) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(true, false));
  }

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFileSizeLimitNotExceeded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "There lies the port; the vessel puffs her sail:";
  const size_t file_size_limit_bytes = log.length() / 2;

  ASSERT_TRUE(EnableLocalLogging(file_size_limit_bytes));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Failure is reported, because not everything could be written. The file
  // will also be closed.
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, false));

  // Additional calls to Write() have no effect.
  ASSERT_EQ(OnWebRtcEventLogWrite(key, "ignored"),
            std::make_pair(false, false));

  ExpectLocalFileContents(*file_path, std::string());
}

TEST_F(WebRtcEventLogManagerTest, LocalLogSanityOverUnlimitedFileSizes) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging(kWebRtcEventLogManagerUnlimitedFileSize));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log1 = "Who let the dogs out?";
  const std::string log2 = "Woof, woof, woof, woof, woof!";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log1), std::make_pair(true, false));
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log2), std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(*file_path, log1 + log2);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogNoWriteAfterLogStopped) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string log_before = "log_before_stop";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log_before),
            std::make_pair(true, false));
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  const std::string log_after = "log_after_stop";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log_after),
            std::make_pair(false, false));

  ExpectLocalFileContents(*file_path, log_before);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogOnlyWritesTheLogsAfterStarted) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Calls to Write() before the log was started are ignored.
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  const std::string log1 = "The lights begin to twinkle from the rocks:";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log1), std::make_pair(false, false));
  ASSERT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));

  std::optional<base::FilePath> file_path;
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // Calls after the log started have an effect. The calls to Write() from
  // before the log started are not remembered.
  const std::string log2 = "The long day wanes: the slow moon climbs: the deep";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log2), std::make_pair(true, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(*file_path, log2);
}

// Note: This test also covers the scenario LocalLogExistingFilesNotOverwritten,
// which is therefore not explicitly tested.
TEST_F(WebRtcEventLogManagerTest, LocalLoggingRestartCreatesNewFile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  const std::vector<std::string> logs = {"<setup>", "<punchline>", "<encore>"};
  std::vector<std::optional<PeerConnectionKey>> keys(logs.size());
  std::vector<std::optional<base::FilePath>> file_paths(logs.size());

  ASSERT_TRUE(OnPeerConnectionAdded(key));

  for (size_t i = 0; i < logs.size(); ++i) {
    ON_CALL(local_observer_, OnLocalLogStarted(_, _))
        .WillByDefault(Invoke(SaveKeyAndFilePathTo(&keys[i], &file_paths[i])));
    ASSERT_TRUE(EnableLocalLogging());
    ASSERT_TRUE(keys[i]);
    ASSERT_EQ(*keys[i], key);
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
    ASSERT_EQ(OnWebRtcEventLogWrite(key, logs[i]), std::make_pair(true, false));
    ASSERT_TRUE(DisableLocalLogging());
  }

  for (size_t i = 0; i < logs.size(); ++i) {
    ExpectLocalFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogMultipleActiveFiles) {
  ASSERT_TRUE(EnableLocalLogging());

  std::list<MockRenderProcessHost> rphs;
  for (size_t i = 0; i < 3; ++i) {
    rphs.emplace_back(browser_context_.get());  // (MockRenderProcessHost ctor)
  }

  std::vector<PeerConnectionKey> keys;
  for (auto& rph : rphs) {
    keys.push_back(GetPeerConnectionKey(&rph, kLid));
  }

  std::vector<std::optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    ON_CALL(local_observer_, OnLocalLogStarted(keys[i], _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(OnPeerConnectionAdded(keys[i]));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); ++i) {
    logs.emplace_back(base::NumberToString(rph_->GetID()) +
                      base::NumberToString(kLid));
    ASSERT_EQ(OnWebRtcEventLogWrite(keys[i], logs[i]),
              std::make_pair(true, false));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  ASSERT_TRUE(DisableLocalLogging());

  for (size_t i = 0; i < keys.size(); ++i) {
    ExpectLocalFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, LocalLogLimitActiveLocalLogFiles) {
  ASSERT_TRUE(EnableLocalLogging());

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
  }

  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  const auto last_key = GetPeerConnectionKey(rph_.get(), kMaxLocalLogFiles);
  ASSERT_TRUE(OnPeerConnectionAdded(last_key));
}

// When a log reaches its maximum size limit, it is closed, and no longer
// counted towards the limit.
TEST_F(WebRtcEventLogManagerTest, LocalLogFilledLogNotCountedTowardsLogsLimit) {
  const std::string log = "very_short_log";
  ASSERT_TRUE(EnableLocalLogging(log.size()));

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
  }

  // By writing to one of the logs, we fill it and end up closing it, allowing
  // an additional log to be written.
  const auto removed_key = GetPeerConnectionKey(rph_.get(), 0);
  EXPECT_EQ(OnWebRtcEventLogWrite(removed_key, log),
            std::make_pair(true, false));

  // We now have room for one additional log.
  const auto last_key = GetPeerConnectionKey(rph_.get(), kMaxLocalLogFiles);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(last_key));
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogForRemovedPeerConnectionNotCountedTowardsLogsLimit) {
  ASSERT_TRUE(EnableLocalLogging());

  const int kMaxLocalLogFiles =
      static_cast<int>(kMaxNumberLocalWebRtcEventLogFiles);
  for (int i = 0; i < kMaxLocalLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _)).Times(1);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
  }

  // When one peer connection is removed, one log is stopped, thereby allowing
  // an additional log to be opened.
  const auto removed_key = GetPeerConnectionKey(rph_.get(), 0);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(removed_key)).Times(1);
  ASSERT_TRUE(OnPeerConnectionRemoved(removed_key));

  // We now have room for one additional log.
  const auto last_key = GetPeerConnectionKey(rph_.get(), kMaxLocalLogFiles);
  EXPECT_CALL(local_observer_, OnLocalLogStarted(last_key, _)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(last_key));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogIllegalPath) {
  // Since the log file won't be properly opened, these will not be called.
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));

  // See the documentation of the function for why |true| is expected despite
  // the path being illegal.
  const base::FilePath illegal_path(FILE_PATH_LITERAL(":!@#$%|`^&*\\/"));
  EXPECT_TRUE(EnableLocalLogging(illegal_path));

  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(WebRtcEventLogManagerTest, LocalLogLegalPathWithoutPermissionsSanity) {
  RemoveWritePermissions(local_logs_base_dir_.GetPath());

  // Since the log file won't be properly opened, these will not be called.
  EXPECT_CALL(local_observer_, OnLocalLogStarted(_, _)).Times(0);
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));

  // See the documentation of the function for why |true| is expected despite
  // the path being illegal.
  EXPECT_TRUE(EnableLocalLogging(local_logs_base_path_));

  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));

  // Write() has no effect (but is handled gracefully).
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "Why did the chicken cross the road?"),
            std::make_pair(false, false));
  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));

  // Logging was enabled, even if it had no effect because of the lacking
  // permissions; therefore, the operation of disabling it makes sense.
  EXPECT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(base::IsDirectoryEmpty(local_logs_base_dir_.GetPath()));
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(WebRtcEventLogManagerTest, LocalLogEmptyStringHandledGracefully) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "<encore>"};

  std::optional<base::FilePath> file_path;

  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  for (size_t i = 0; i < logs.size(); ++i) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key, logs[i]), std::make_pair(true, false));
  }
  ASSERT_TRUE(DisableLocalLogging());

  ExpectLocalFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest, LocalLogFilenameMatchesExpectedFormat) {
  using StringType = base::FilePath::StringType;

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  static constexpr base::Time::Exploded kFrozenTime = {.year = 2017,
                                                       .month = 9,
                                                       .day_of_week = 3,
                                                       .day_of_month = 6,
                                                       .hour = 10,
                                                       .minute = 43,
                                                       .second = 29};
  ASSERT_TRUE(kFrozenTime.HasValidValues());
  FreezeClockAt(kFrozenTime);

  const StringType user_defined = FILE_PATH_LITERAL("user_defined");
  const base::FilePath local_logs_base_path =
      local_logs_base_dir_.GetPath().Append(user_defined);

  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // [user_defined]_[date]_[time]_[render_process_id]_[lid].[extension]
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path = local_logs_base_path;
  expected_path = local_logs_base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + NumberToStringType(rph_->GetID()) +
      FILE_PATH_LITERAL("_") + NumberToStringType(kLid));
  expected_path = expected_path.AddExtension(local_log_extension_);

  EXPECT_EQ(file_path, expected_path);
}

TEST_F(WebRtcEventLogManagerTest,
       LocalLogFilenameMatchesExpectedFormatRepeatedFilename) {
  using StringType = base::FilePath::StringType;

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path_1;
  std::optional<base::FilePath> file_path_2;
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillOnce(Invoke(SaveFilePathTo(&file_path_1)))
      .WillOnce(Invoke(SaveFilePathTo(&file_path_2)));

  static constexpr base::Time::Exploded kFrozenTime = {.year = 2017,
                                                       .month = 9,
                                                       .day_of_week = 3,
                                                       .day_of_month = 6,
                                                       .hour = 10,
                                                       .minute = 43,
                                                       .second = 29};
  ASSERT_TRUE(kFrozenTime.HasValidValues());
  FreezeClockAt(kFrozenTime);

  const StringType user_defined_portion = FILE_PATH_LITERAL("user_defined");
  const base::FilePath local_logs_base_path =
      local_logs_base_dir_.GetPath().Append(user_defined_portion);

  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(file_path_1);
  ASSERT_FALSE(file_path_1->empty());

  // [user_defined]_[date]_[time]_[render_process_id]_[lid].[extension]
  const StringType date = FILE_PATH_LITERAL("20170906");
  const StringType time = FILE_PATH_LITERAL("1043");
  base::FilePath expected_path_1 = local_logs_base_path;
  expected_path_1 = local_logs_base_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("_") + date + FILE_PATH_LITERAL("_") + time +
      FILE_PATH_LITERAL("_") + NumberToStringType(rph_->GetID()) +
      FILE_PATH_LITERAL("_") + NumberToStringType(kLid));
  expected_path_1 = expected_path_1.AddExtension(local_log_extension_);

  ASSERT_EQ(file_path_1, expected_path_1);

  ASSERT_TRUE(DisableLocalLogging());
  ASSERT_TRUE(EnableLocalLogging(local_logs_base_path));
  ASSERT_TRUE(file_path_2);
  ASSERT_FALSE(file_path_2->empty());

  const base::FilePath expected_path_2 =
      expected_path_1.InsertBeforeExtension(FILE_PATH_LITERAL(" (1)"));

  // Focus of the test - starting the same log again produces a new file,
  // with an expected new filename.
  ASSERT_EQ(file_path_2, expected_path_2);
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStartedNotCalledIfRemoteLoggingNotEnabled) {
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(_, _, _)).Times(0);
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_TRUE(OnPeerConnectionSessionIdSet(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStoppedNotCalledIfRemoteLoggingNotEnabled) {
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(_)).Times(0);
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  EXPECT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStartedCalledIfRemoteLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnRemoteLogStoppedCalledIfRemoteLoggingEnabledThenPcRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest,
       BrowserContextInitializationCreatesDirectoryForRemoteLogs) {
  auto browser_context = CreateBrowserContext();
  const base::FilePath remote_logs_path =
      RemoteBoundLogsDir(browser_context.get());
  EXPECT_TRUE(base::DirectoryExists(remote_logs_path));
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnknownPeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), 0);
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, "id", nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureUnknownOrInactivePeerConnection);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnknownSessionId) {
  const auto key = GetPeerConnectionKey(rph_.get(), 0);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, "wrong_id", nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureUnknownOrInactivePeerConnection);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsTrueIfKnownSessionId) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  EXPECT_TRUE(StartRemoteLogging(key, kSessionId));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfRestartAttempt) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  ASSERT_TRUE(StartRemoteLogging(key, kSessionId));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, kSessionId, nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureAlreadyLogging);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfUnlimitedFileSize) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, kSessionId,
                                  kWebRtcEventLogManagerUnlimitedFileSize, 0,
                                  kWebAppId, nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureUnlimitedSizeDisallowed);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsTrueIfFileSizeAtOrBelowLimit) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  EXPECT_TRUE(StartRemoteLogging(key, kSessionId, kMaxRemoteLogFileSizeBytes, 0,
                                 kWebAppId));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfFileSizeToSmall) {
  const size_t min_size =
      CreateLogFileWriterFactory(Compression::GZIP_NULL_ESTIMATION)
          ->MinFileSizeBytes();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, kSessionId, min_size - 1, 0, kWebAppId,
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureMaxSizeTooSmall);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfExcessivelyLargeFileSize) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, kSessionId,
                                  kMaxRemoteLogFileSizeBytes + 1, 0, kWebAppId,
                                  nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureMaxSizeTooLarge);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfExcessivelyLargeOutputPeriodMs) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, kSessionId, kMaxRemoteLogFileSizeBytes,
                                  kMaxOutputPeriodMs + 1, kWebAppId, nullptr,
                                  &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureOutputPeriodMsTooLarge);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsFalseIfPeerConnectionAlreadyClosed) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, kSessionId, nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureUnknownOrInactivePeerConnection);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingDoesNotReturnIdWhenUnsuccessful) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  std::string log_id;
  ASSERT_FALSE(StartRemoteLogging(key, kSessionId, &log_id));

  EXPECT_TRUE(log_id.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingReturnsLegalIdWhenSuccessful) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));

  std::string log_id;
  ASSERT_TRUE(StartRemoteLogging(key, kSessionId, &log_id));

  EXPECT_EQ(log_id.size(), 32u);
  EXPECT_EQ(log_id.find_first_not_of("0123456789ABCDEF"), std::string::npos);
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingSavesToFileWithCorrectFileNameFormat) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::string log_id;
  ASSERT_TRUE(StartRemoteLogging(key, &log_id));

  // Compare filename (without extension).
  const std::string filename =
      file_path->BaseName().RemoveExtension().MaybeAsASCII();
  ASSERT_FALSE(filename.empty());

  const std::string expected_filename =
      std::string(kRemoteBoundWebRtcEventLogFileNamePrefix) + "_" +
      base::NumberToString(kWebAppId) + "_" + log_id;
  EXPECT_EQ(filename, expected_filename);

  // Compare extension.
  EXPECT_EQ(base::FilePath::kExtensionSeparator +
                base::FilePath::StringType(remote_log_extension_),
            file_path->Extension());
}

TEST_F(WebRtcEventLogManagerTest, StartRemoteLoggingCreatesEmptyFile) {
  std::optional<base::FilePath> file_path;
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));

  // Close file before examining its contents.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectRemoteFileContents(*file_path, std::string());
}

// TODO(crbug.com/40752893): Fix this flaky test.
TEST_F(WebRtcEventLogManagerTest,
       DISABLED_RemoteLogFileCreatedInCorrectDirectory) {
  // Set up separate browser contexts; each one will get one log.
  constexpr size_t kLogsNum = 3;
  std::unique_ptr<TestingProfile> browser_contexts[kLogsNum];
  std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
  for (size_t i = 0; i < kLogsNum; ++i) {
    browser_contexts[i] = CreateBrowserContext();
    rphs.emplace_back(
        std::make_unique<MockRenderProcessHost>(browser_contexts[i].get()));
  }

  // Prepare to store the logs' paths in distinct memory locations.
  std::optional<base::FilePath> file_paths[kLogsNum];
  for (size_t i = 0; i < kLogsNum; ++i) {
    const auto key = GetPeerConnectionKey(rphs[i].get(), kLid);
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
        .Times(1)
        .WillOnce(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  // Start one log for each browser context.
  for (const auto& rph : rphs) {
    const auto key = GetPeerConnectionKey(&*rph, kLid);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
    ASSERT_TRUE(StartRemoteLogging(key));
  }

  // All log files must be created in their own context's directory.
  for (size_t i = 0; i < std::size(browser_contexts); ++i) {
    ASSERT_TRUE(file_paths[i]);
    EXPECT_TRUE(browser_contexts[i]->GetPath().IsParent(*file_paths[i]));
  }
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingSanityIfDuplicateIdsInDifferentRendererProcesses) {
  std::unique_ptr<MockRenderProcessHost> rphs[2] = {
      std::make_unique<MockRenderProcessHost>(browser_context_.get()),
      std::make_unique<MockRenderProcessHost>(browser_context_.get()),
  };

  PeerConnectionKey keys[2] = {GetPeerConnectionKey(rphs[0].get(), 0),
                               GetPeerConnectionKey(rphs[1].get(), 0)};

  // The ID is shared, but that's not a problem, because the renderer process
  // are different.
  const std::string id = "shared_id";
  ASSERT_TRUE(OnPeerConnectionAdded(keys[0]));
  OnPeerConnectionSessionIdSet(keys[0], id);
  ASSERT_TRUE(OnPeerConnectionAdded(keys[1]));
  OnPeerConnectionSessionIdSet(keys[1], id);

  // Make sure the logs get written to separate files.
  std::optional<base::FilePath> file_paths[2];
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _, _))
        .Times(1)
        .WillOnce(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  EXPECT_TRUE(StartRemoteLogging(keys[0], id));
  EXPECT_TRUE(StartRemoteLogging(keys[1], id));

  EXPECT_TRUE(file_paths[0]);
  EXPECT_TRUE(file_paths[1]);
  EXPECT_NE(file_paths[0], file_paths[1]);
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteWritesToTheRemoteBoundFile) {
  std::optional<base::FilePath> file_path;
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));

  const char* const log = "1 + 1 = 3";
  EXPECT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, true));

  // Close file before examining its contents.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectRemoteFileContents(*file_path, log);
}

TEST_F(WebRtcEventLogManagerTest, WriteToBothLocalAndRemoteFiles) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> local_path;
  EXPECT_CALL(local_observer_, OnLocalLogStarted(key, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&local_path)));

  std::optional<base::FilePath> remote_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&remote_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key));

  ASSERT_TRUE(local_path);
  ASSERT_FALSE(local_path->empty());
  ASSERT_TRUE(remote_path);
  ASSERT_FALSE(remote_path->empty());

  const char* const log = "logloglog";
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(true, true));

  // Ensure the flushing of the file to disk before attempting to read them.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(*local_path, log);
  ExpectRemoteFileContents(*remote_path, log);
}

TEST_F(WebRtcEventLogManagerTest, MultipleWritesToSameRemoteBoundLogfile) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  const std::string logs[] = {"ABC", "DEF", "XYZ"};
  for (const std::string& log : logs) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, true));
  }

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectRemoteFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogFileSizeLimitNotExceededSingleWrite) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "tpyo";

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  ASSERT_TRUE(
      StartRemoteLogging(key, kSessionId, GzippedSize(log) - 1, 0, kWebAppId));

  // Failure is reported, because not everything could be written. The file
  // will also be closed.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, false));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  // No partial writes occurred.
  ExpectRemoteFileContents(*file_path, std::string());
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogFileSizeLimitNotExceededMultipleWrites) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log1 = "abcabc";
  const std::string log2 = "defghijklmnopqrstuvwxyz";

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key, kSessionId));
  ASSERT_TRUE(
      StartRemoteLogging(key, kSessionId, 1 + GzippedSize(log1), 0, kWebAppId));

  // First write works.
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log1), std::make_pair(false, true));

  // On the second write, failure is reported, because not everything could be
  // written. The file will also be closed.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ASSERT_EQ(OnWebRtcEventLogWrite(key, log2), std::make_pair(false, false));

  ExpectRemoteFileContents(*file_path, log1);
}

TEST_F(WebRtcEventLogManagerTest,
       LogMultipleActiveRemoteLogsSameBrowserContext) {
  const std::vector<PeerConnectionKey> keys = {
      GetPeerConnectionKey(rph_.get(), 0), GetPeerConnectionKey(rph_.get(), 1),
      GetPeerConnectionKey(rph_.get(), 2)};

  std::vector<std::optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    ON_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _, _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(OnPeerConnectionAdded(keys[i]));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(keys[i]));
    ASSERT_TRUE(StartRemoteLogging(keys[i]));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); ++i) {
    logs.emplace_back(base::NumberToString(rph_->GetID()) +
                      base::NumberToString(i));
    ASSERT_EQ(OnWebRtcEventLogWrite(keys[i], logs[i]),
              std::make_pair(false, true));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  for (auto& key : keys) {
    ASSERT_TRUE(OnPeerConnectionRemoved(key));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    ExpectRemoteFileContents(*file_paths[i], logs[i]);
  }
}

// TODO(crbug.com/40709493): Fix this flaky test.
TEST_F(WebRtcEventLogManagerTest,
       DISABLED_LogMultipleActiveRemoteLogsDifferentBrowserContexts) {
  constexpr size_t kLogsNum = 3;
  std::unique_ptr<TestingProfile> browser_contexts[kLogsNum];
  std::vector<std::unique_ptr<MockRenderProcessHost>> rphs;
  for (size_t i = 0; i < kLogsNum; ++i) {
    browser_contexts[i] = CreateBrowserContext();
    rphs.emplace_back(
        std::make_unique<MockRenderProcessHost>(browser_contexts[i].get()));
  }

  std::vector<PeerConnectionKey> keys;
  for (auto& rph : rphs) {
    keys.push_back(GetPeerConnectionKey(rph.get(), kLid));
  }

  std::vector<std::optional<base::FilePath>> file_paths(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    ON_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _, _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
    ASSERT_TRUE(OnPeerConnectionAdded(keys[i]));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(keys[i]));
    ASSERT_TRUE(StartRemoteLogging(keys[i]));
    ASSERT_TRUE(file_paths[i]);
    ASSERT_FALSE(file_paths[i]->empty());
  }

  std::vector<std::string> logs;
  for (size_t i = 0; i < keys.size(); ++i) {
    logs.emplace_back(base::NumberToString(rph_->GetID()) +
                      base::NumberToString(i));
    ASSERT_EQ(OnWebRtcEventLogWrite(keys[i], logs[i]),
              std::make_pair(false, true));
  }

  // Make sure the file woulds be closed, so that we could safely read them.
  for (auto& key : keys) {
    ASSERT_TRUE(OnPeerConnectionRemoved(key));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    ExpectRemoteFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, DifferentRemoteLogsMayHaveDifferentMaximums) {
  const std::string logs[2] = {"abra", "cadabra"};
  std::vector<std::optional<base::FilePath>> file_paths(std::size(logs));
  std::vector<PeerConnectionKey> keys;
  for (size_t i = 0; i < std::size(logs); ++i) {
    keys.push_back(GetPeerConnectionKey(rph_.get(), i));
    ON_CALL(remote_observer_, OnRemoteLogStarted(keys[i], _, _))
        .WillByDefault(Invoke(SaveFilePathTo(&file_paths[i])));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    ASSERT_TRUE(OnPeerConnectionAdded(keys[i]));
    const std::string session_id = GetUniqueId(keys[i]);
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(keys[i], session_id));
    ASSERT_TRUE(StartRemoteLogging(keys[i], session_id, GzippedSize(logs[i]), 0,
                                   kWebAppId));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    // The write is successful, but the file closed, indicating that the
    // maximum file size has been reached.
    EXPECT_CALL(remote_observer_, OnRemoteLogStopped(keys[i])).Times(1);
    ASSERT_EQ(OnWebRtcEventLogWrite(keys[i], logs[i]),
              std::make_pair(false, true));
    ASSERT_TRUE(file_paths[i]);
    ExpectRemoteFileContents(*file_paths[i], logs[i]);
  }
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogFileClosedWhenCapacityReached) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  const std::string log = "Let X equal X.";

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key, GetUniqueId(key), GzippedSize(log), 0,
                                 kWebAppId));
  ASSERT_TRUE(file_path);

  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  EXPECT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, true));
}

#if BUILDFLAG(IS_POSIX)
// TODO(crbug.com/40545136): Add unit tests for lacking read permissions when
// looking to upload the file.
TEST_F(WebRtcEventLogManagerTest,
       FailureToCreateRemoteLogsDirHandledGracefully) {
  const base::FilePath browser_context_dir = browser_context_->GetPath();
  const base::FilePath remote_logs_path =
      RemoteBoundLogsDir(browser_context_.get());

  // Unload the profile, delete its remove logs directory, and remove write
  // permissions from it, thereby preventing it from being created again.
  UnloadMainTestProfile();
  ASSERT_TRUE(base::DeletePathRecursively(remote_logs_path));
  RemoveWritePermissions(browser_context_dir);

  // Graceful handling by BrowserContext::EnableForBrowserContext, despite
  // failing to create the remote logs' directory..
  LoadMainTestProfile();
  EXPECT_FALSE(base::DirectoryExists(remote_logs_path));

  // Graceful handling of OnPeerConnectionAdded: True returned because the
  // remote-logs' manager can still safely reason about the state of peer
  // connections even if one of its browser contexts is defective.)
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  // Graceful handling of StartRemoteLogging: False returned because it's
  // impossible to write the log to a file.
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureLoggingDisabledBrowserContext);

  // Graceful handling of OnWebRtcEventLogWrite: False returned because the
  // log could not be written at all, let alone in its entirety.
  const char* const log = "This is not a log.";
  EXPECT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, false));

  // Graceful handling of OnPeerConnectionRemoved: True returned because the
  // remote-logs' manager can still safely reason about the state of peer
  // connections even if one of its browser contexts is defective.
  EXPECT_TRUE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest, GracefullyHandleFailureToStartRemoteLogFile) {
  // WebRTC logging will not be turned on.
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(_, _, _)).Times(0);
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(_)).Times(0);

  // Remove write permissions from the directory.
  const base::FilePath remote_logs_path =
      RemoteBoundLogsDir(browser_context_.get());
  ASSERT_TRUE(base::DirectoryExists(remote_logs_path));
  RemoveWritePermissions(remote_logs_path);

  // StartRemoteLogging() will now fail.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureFileCreationError);
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "abc"), std::make_pair(false, false));
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(WebRtcEventLogManagerTest, RemoteLogLimitActiveLogFiles) {
  for (int i = 0; i < kMaxActiveRemoteLogFiles + 1; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  }

  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key));
  }

  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(_, _, _)).Times(0);
  const auto new_key =
      GetPeerConnectionKey(rph_.get(), kMaxActiveRemoteLogFiles);
  EXPECT_FALSE(StartRemoteLogging(new_key));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogFilledLogNotCountedTowardsLogsLimit) {
  const std::string log = "very_short_log";

  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key, GetUniqueId(key), GzippedSize(log), 0,
                                   kWebAppId));
  }

  // By writing to one of the logs until it reaches capacity, we fill it,
  // causing it to close, therefore allowing an additional log.
  const auto removed_key = GetPeerConnectionKey(rph_.get(), 0);
  EXPECT_EQ(OnWebRtcEventLogWrite(removed_key, log),
            std::make_pair(false, true));

  // We now have room for one additional log.
  const auto new_key =
      GetPeerConnectionKey(rph_.get(), kMaxActiveRemoteLogFiles);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(new_key, _, _)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(new_key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(new_key));
  ASSERT_TRUE(StartRemoteLogging(new_key));
}

TEST_F(WebRtcEventLogManagerTest,
       RemoteLogForRemovedPeerConnectionNotCountedTowardsLogsLimit) {
  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
    EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _)).Times(1);
    ASSERT_TRUE(StartRemoteLogging(key));
  }

  // By removing a peer connection associated with one of the logs, we allow
  // an additional log.
  const auto removed_key = GetPeerConnectionKey(rph_.get(), 0);
  ASSERT_TRUE(OnPeerConnectionRemoved(removed_key));

  // We now have room for one additional log.
  const auto last_key =
      GetPeerConnectionKey(rph_.get(), kMaxActiveRemoteLogFiles);
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(last_key, _, _)).Times(1);
  ASSERT_TRUE(OnPeerConnectionAdded(last_key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(last_key));
  ASSERT_TRUE(StartRemoteLogging(last_key));
}

TEST_F(WebRtcEventLogManagerTest,
       ActiveLogsForBrowserContextCountedTowardsItsPendingsLogsLimit) {
  SuppressUploading();

  // Produce kMaxPendingRemoteLogFiles pending logs.
  for (int i = 0; i < kMaxPendingRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rph_.get(), i);
    ASSERT_TRUE(OnPeerConnectionAdded(key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
    ASSERT_TRUE(StartRemoteLogging(key));
    ASSERT_TRUE(OnPeerConnectionRemoved(key));
  }

  // It is now impossible to start another *active* log for that BrowserContext,
  // because we have too many pending logs (and active logs become pending
  // once completed).
  const auto forbidden =
      GetPeerConnectionKey(rph_.get(), kMaxPendingRemoteLogFiles);
  ASSERT_TRUE(OnPeerConnectionAdded(forbidden));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(forbidden));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(forbidden, nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureNoAdditionalActiveLogsAllowed);
}

TEST_F(WebRtcEventLogManagerTest,
       ObserveLimitOnMaximumPendingLogsPerBrowserContext) {
  SuppressUploading();

  // Create additional BrowserContexts for the test.
  std::unique_ptr<TestingProfile> browser_contexts[2] = {
      CreateBrowserContext(), CreateBrowserContext()};
  std::unique_ptr<MockRenderProcessHost> rphs[2] = {
      std::make_unique<MockRenderProcessHost>(browser_contexts[0].get()),
      std::make_unique<MockRenderProcessHost>(browser_contexts[1].get())};

  // Allowed to start kMaxPendingRemoteLogFiles for each BrowserContext.
  // Specifically, we can do it for the first BrowserContext.
  for (int i = 0; i < kMaxPendingRemoteLogFiles; ++i) {
    const auto key = GetPeerConnectionKey(rphs[0].get(), i);
    // The log could be opened:
    ASSERT_TRUE(OnPeerConnectionAdded(key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
    ASSERT_TRUE(StartRemoteLogging(key));
    // The log changes state from ACTIVE to PENDING:
    EXPECT_TRUE(OnPeerConnectionRemoved(key));
  }

  // Not allowed to start any more remote-bound logs for the BrowserContext on
  // which the limit was reached.
  const auto key0 =
      GetPeerConnectionKey(rphs[0].get(), kMaxPendingRemoteLogFiles);
  ASSERT_TRUE(OnPeerConnectionAdded(key0));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key0));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key0, nullptr, &error_message));
  EXPECT_EQ(error_message,
            kStartRemoteLoggingFailureNoAdditionalActiveLogsAllowed);

  // Other BrowserContexts aren't limit by the previous one's limit.
  const auto key1 = GetPeerConnectionKey(rphs[1].get(), 0);
  ASSERT_TRUE(OnPeerConnectionAdded(key1));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key1));
  EXPECT_TRUE(StartRemoteLogging(key1));
}

// This also tests the scenario UploadOrderDependsOnLastModificationTime.
TEST_F(WebRtcEventLogManagerTest,
       LogsFromPreviousSessionBecomePendingLogsWhenBrowserContextInitialized) {
  // Unload the profile, but remember where it stores its files.
  const base::FilePath browser_context_path = browser_context_->GetPath();
  const base::FilePath remote_logs_dir =
      RemoteBoundLogsDir(browser_context_.get());
  UnloadMainTestProfile();

  // Seed the remote logs' directory with log files, simulating the
  // creation of logs in a previous session.
  std::list<WebRtcLogFileInfo> expected_files;
  ASSERT_TRUE(base::CreateDirectory(remote_logs_dir));

  // Avoid arbitrary ordering due to files being created in the same second.
  // This is OK in production, but can confuse the test, which expects a
  // specific order.
  base::Time time =
      base::Time::Now() - base::Seconds(kMaxPendingRemoteBoundWebRtcEventLogs);

  for (size_t i = 0; i < kMaxPendingRemoteBoundWebRtcEventLogs; ++i) {
    time += base::Seconds(1);

    base::FilePath file_path;
    base::File file;
    ASSERT_TRUE(CreateRemoteBoundLogFile(remote_logs_dir, kWebAppId,
                                         remote_log_extension_, time,
                                         &file_path, &file));

    expected_files.emplace_back(browser_context_id_, file_path, time);
  }

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  LoadMainTestProfile();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path);

  WaitForPendingTasks(&run_loop);
}

// It is possible for remote-bound logs to be compressed or uncompressed.
// We show that logs from a previous session are captured even if they are
// different, with regards to compression, compared to last time.
TEST_F(WebRtcEventLogManagerTest,
       LogsCapturedPreviouslyMadePendingEvenIfDifferentExtensionsUsed) {
  // Unload the profile, but remember where it stores its files.
  const base::FilePath browser_context_path = browser_context_->GetPath();
  const base::FilePath remote_logs_dir =
      RemoteBoundLogsDir(browser_context_.get());
  UnloadMainTestProfile();

  // Seed the remote logs' directory with log files, simulating the
  // creation of logs in a previous session.
  std::list<WebRtcLogFileInfo> expected_files;
  ASSERT_TRUE(base::CreateDirectory(remote_logs_dir));

  base::FilePath::StringPieceType extensions[] = {
      kWebRtcEventLogUncompressedExtension, kWebRtcEventLogGzippedExtension};
  ASSERT_LE(std::size(extensions), kMaxPendingRemoteBoundWebRtcEventLogs)
      << "Lacking test coverage.";

  // Avoid arbitrary ordering due to files being created in the same second.
  // This is OK in production, but can confuse the test, which expects a
  // specific order.
  base::Time time =
      base::Time::Now() - base::Seconds(kMaxPendingRemoteBoundWebRtcEventLogs);

  for (size_t i = 0, ext = 0; i < kMaxPendingRemoteBoundWebRtcEventLogs; ++i) {
    time += base::Seconds(1);

    const auto& extension = extensions[ext];
    ext = (ext + 1) % std::size(extensions);

    base::FilePath file_path;
    base::File file;
    ASSERT_TRUE(CreateRemoteBoundLogFile(remote_logs_dir, kWebAppId, extension,
                                         time, &file_path, &file));

    expected_files.emplace_back(browser_context_id_, file_path, time);
  }

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  LoadMainTestProfile();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path);

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTest,
       WhenOnPeerConnectionRemovedFinishedRemoteLogUploadedAndFileDeleted) {
  // |upload_result| show that the files are deleted independent of the
  // upload's success / failure.
  const bool upload_result = GetParam();

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, upload_result, &run_loop));

  // Peer connection removal triggers next upload.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  WaitForPendingTasks(&run_loop);

  EXPECT_TRUE(
      base::IsDirectoryEmpty(RemoteBoundLogsDir(browser_context_.get())));
}

TEST_P(WebRtcEventLogManagerTest, DestroyedRphTriggersLogUpload) {
  // |upload_result| show that the files are deleted independent of the
  // upload's success / failure.
  const bool upload_result = GetParam();

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, upload_result, &run_loop));

  // RPH destruction stops all active logs and triggers next upload.
  rph_.reset();

  WaitForPendingTasks(&run_loop);

  EXPECT_TRUE(
      base::IsDirectoryEmpty(RemoteBoundLogsDir(browser_context_.get())));
}

// Note that SuppressUploading() and UnSuppressUploading() use the behavior
// guaranteed by this test.
TEST_F(WebRtcEventLogManagerTest, UploadOnlyWhenNoActivePeerConnections) {
  const auto untracked = GetPeerConnectionKey(rph_.get(), 0);
  const auto tracked = GetPeerConnectionKey(rph_.get(), 1);

  // Suppresses the uploading of the "tracked" peer connection's log.
  ASSERT_TRUE(OnPeerConnectionAdded(untracked));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(untracked));

  // The tracked peer connection's log is not uploaded when finished, because
  // another peer connection is still active.
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(tracked, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(tracked));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(tracked));
  ASSERT_TRUE(StartRemoteLogging(tracked));
  ASSERT_TRUE(log_file);
  ASSERT_TRUE(OnPeerConnectionRemoved(tracked));

  // Perform another action synchronously, so that we may be assured that the
  // observer's lack of callbacks was not a timing fluke.
  OnWebRtcEventLogWrite(untracked, "Ook!");

  // Having been convinced that |tracked|'s log was not uploded while
  // |untracked| was active, close |untracked| and see that |tracked|'s log
  // is now uploaded.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_uploads = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_uploads, true, &run_loop));
  ASSERT_TRUE(OnPeerConnectionRemoved(untracked));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTest, ExpiredFilesArePrunedRatherThanUploaded) {
  constexpr size_t kExpired = 0;
  constexpr size_t kFresh = 1;
  DCHECK_GE(kMaxPendingRemoteBoundWebRtcEventLogs, 2u)
      << "Please restructure the test to use separate browser contexts.";

  const base::FilePath remote_logs_dir =
      RemoteBoundLogsDir(browser_context_.get());

  UnloadMainTestProfile();

  base::FilePath file_paths[2];
  for (size_t i = 0; i < 2; ++i) {
    base::File file;
    ASSERT_TRUE(CreateRemoteBoundLogFile(
        remote_logs_dir, kWebAppId, remote_log_extension_, base::Time::Now(),
        &file_paths[i], &file));
  }

  // Touch() requires setting the last access time as well. Keep it current,
  // showing that only the last modification time matters.
  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_paths[0], &file_info));

  // Set the expired file's last modification time to past max retention.
  const base::Time expired_mod_time = base::Time::Now() -
                                      kRemoteBoundWebRtcEventLogsMaxRetention -
                                      base::Seconds(1);
  ASSERT_TRUE(base::TouchFile(file_paths[kExpired], file_info.last_accessed,
                              expired_mod_time));

  // Show that the expired file is not uploaded.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {
      WebRtcLogFileInfo(browser_context_id_, file_paths[kFresh],
                        GetLastModificationTime(file_paths[kFresh]))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Recognize the files as pending by initializing their BrowserContext.
  LoadMainTestProfile();

  WaitForPendingTasks(&run_loop);

  // Both the uploaded file as well as the expired file have no been removed
  // from local disk.
  for (const base::FilePath& file_path : file_paths) {
    EXPECT_FALSE(base::PathExists(file_path));
  }
}

// TODO(crbug.com/40545136): Add a test showing that a file expiring while
// another is being uploaded, is not uploaded after the current upload is
// completed. This is significant because Chrome might stay up for a long time.

TEST_F(WebRtcEventLogManagerTest, RemoteLogEmptyStringHandledGracefully) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // By writing a log after the empty string, we show that no odd behavior is
  // encountered, such as closing the file (an actual bug from WebRTC).
  const std::vector<std::string> logs = {"<setup>", "", "<encore>"};

  std::optional<base::FilePath> file_path;

  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  for (size_t i = 0; i < logs.size(); ++i) {
    ASSERT_EQ(OnWebRtcEventLogWrite(key, logs[i]), std::make_pair(false, true));
  }
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectRemoteFileContents(
      *file_path,
      std::accumulate(std::begin(logs), std::end(logs), std::string()));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(WebRtcEventLogManagerTest,
       UnopenedRemoteLogFilesNotCountedTowardsActiveLogsLimit) {
  std::unique_ptr<TestingProfile> browser_contexts[2];
  std::unique_ptr<MockRenderProcessHost> rphs[2];
  for (size_t i = 0; i < 2; ++i) {
    browser_contexts[i] = CreateBrowserContext();
    rphs[i] =
        std::make_unique<MockRenderProcessHost>(browser_contexts[i].get());
  }

  constexpr size_t without_permissions = 0;
  constexpr size_t with_permissions = 1;

  // Remove write permissions from one directory.
  const base::FilePath permissions_lacking_remote_logs_path =
      RemoteBoundLogsDir(browser_contexts[without_permissions].get());
  ASSERT_TRUE(base::DirectoryExists(permissions_lacking_remote_logs_path));
  RemoveWritePermissions(permissions_lacking_remote_logs_path);

  // Fail to start a log associated with the permission-lacking directory.
  const auto without_permissions_key =
      GetPeerConnectionKey(rphs[without_permissions].get(), 0);
  ASSERT_TRUE(OnPeerConnectionAdded(without_permissions_key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(without_permissions_key));
  std::string error;
  ASSERT_FALSE(StartRemoteLogging(without_permissions_key, nullptr, &error));
  EXPECT_EQ(error, kStartRemoteLoggingFailureFileCreationError);

  // Show that this was not counted towards the limit of active files.
  for (int i = 0; i < kMaxActiveRemoteLogFiles; ++i) {
    const auto with_permissions_key =
        GetPeerConnectionKey(rphs[with_permissions].get(), i);
    ASSERT_TRUE(OnPeerConnectionAdded(with_permissions_key));
    ASSERT_TRUE(OnPeerConnectionSessionIdSet(with_permissions_key));
    EXPECT_TRUE(StartRemoteLogging(with_permissions_key));
  }
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(WebRtcEventLogManagerTest,
       NoStartWebRtcSendingEventLogsWhenLocalEnabledWithoutPeerConnection) {
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       NoStartWebRtcSendingEventLogsWhenPeerConnectionButNoLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenLocalEnabledThenOnPeerConnectionAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ExpectWebRtcStateChangeInstruction(key, true);
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenOnPeerConnectionAddedThenLocalEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key, true);
}

TEST_F(WebRtcEventLogManagerTest,
       StartWebRtcSendingEventLogsWhenRemoteLoggingEnabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ExpectWebRtcStateChangeInstruction(key, true);
}

TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenLocalLoggingStopped) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(DisableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key, false);
}

// #1 - Local logging was the cause of the logs.
TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenOnPeerConnectionRemoved1) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ExpectWebRtcStateChangeInstruction(key, false);
}

// #2 - Remote logging was the cause of the logs.
TEST_F(WebRtcEventLogManagerTest,
       InstructWebRtcToStopSendingEventLogsWhenOnPeerConnectionRemoved2) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ExpectWebRtcStateChangeInstruction(key, false);
}

// #1 - Local logging added first.
TEST_F(WebRtcEventLogManagerTest,
       SecondLoggingTargetDoesNotInitiateWebRtcLogging1) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(StartRemoteLogging(key));
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

// #2 - Remote logging added first.
TEST_F(WebRtcEventLogManagerTest,
       SecondLoggingTargetDoesNotInitiateWebRtcLogging2) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

TEST_F(WebRtcEventLogManagerTest,
       DisablingLocalLoggingWhenRemoteLoggingEnabledDoesNotStopWebRtcLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key));
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());

  // Cleanup
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ExpectWebRtcStateChangeInstruction(key, false);
}

TEST_F(WebRtcEventLogManagerTest,
       DisablingLocalLoggingAfterPcRemovalHasNoEffectOnWebRtcLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  // Setup
  SetPeerConnectionTrackerProxyForTesting(
      std::make_unique<PeerConnectionTrackerProxyForTesting>(this));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(StartRemoteLogging(key));
  ExpectWebRtcStateChangeInstruction(key, true);

  // Test
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ExpectWebRtcStateChangeInstruction(key, false);
  ASSERT_TRUE(DisableLocalLogging());
  EXPECT_TRUE(webrtc_state_change_instructions_.empty());
}

// Once a peer connection with a given key was removed, it may not again be
// added. But, if this impossible case occurs, WebRtcEventLogManager will
// not crash.
TEST_F(WebRtcEventLogManagerTest, SanityOverRecreatingTheSamePeerConnection) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  OnWebRtcEventLogWrite(key, "log1");
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  OnWebRtcEventLogWrite(key, "log2");
}

// The logs would typically be binary. However, the other tests only cover ASCII
// characters, for readability. This test shows that this is not a problem.
TEST_F(WebRtcEventLogManagerTest, LogAllPossibleCharacters) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> local_log_file_path;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&local_log_file_path)));

  std::optional<base::FilePath> remote_log_file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&remote_log_file_path)));

  ASSERT_TRUE(EnableLocalLogging());
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(local_log_file_path);
  ASSERT_FALSE(local_log_file_path->empty());
  ASSERT_TRUE(remote_log_file_path);
  ASSERT_FALSE(remote_log_file_path->empty());

  std::string all_chars;
  for (size_t i = 0; i < 256; ++i) {
    all_chars += static_cast<uint8_t>(i);
  }
  ASSERT_EQ(OnWebRtcEventLogWrite(key, all_chars), std::make_pair(true, true));

  // Make sure the file would be closed, so that we could safely read it.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ExpectLocalFileContents(*local_log_file_path, all_chars);
  ExpectRemoteFileContents(*remote_log_file_path, all_chars);
}

TEST_F(WebRtcEventLogManagerTest, LocalLogsClosedWhenRenderProcessHostExits) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());

  // The expectation for OnLocalLogStopped() will be saturated by this
  // destruction of the RenderProcessHost, which triggers an implicit
  // removal of all PeerConnections associated with it.
  EXPECT_CALL(local_observer_, OnLocalLogStopped(key)).Times(1);
  rph_.reset();
}

TEST_F(WebRtcEventLogManagerTest, RemoteLogsClosedWhenRenderProcessHostExits) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));

  // The expectation for OnRemoteLogStopped() will be saturated by this
  // destruction of the RenderProcessHost, which triggers an implicit
  // removal of all PeerConnections associated with it.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  rph_.reset();
}

// Once a RenderProcessHost exits/crashes, its PeerConnections are removed,
// which means that they can no longer suppress an upload.
TEST_F(WebRtcEventLogManagerTest,
       RenderProcessHostExitCanRemoveUploadSuppression) {
  SuppressUploading();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ASSERT_TRUE(file_path);
  ASSERT_FALSE(file_path->empty());

  // The above removal is not sufficient to trigger an upload (so the test will
  // not be flaky). It's only once we destroy the RPH with which the suppressing
  // PeerConnection is associated, that upload will take place.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *file_path, GetLastModificationTime(*file_path))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // We destroy the RPH without explicitly removing its PeerConnection (unlike
  // a call to UnsuppressUploading()).
  upload_suppressing_rph_.reset();

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionAddedOverDestroyedRphReturnsFalse) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  rph_.reset();
  EXPECT_FALSE(OnPeerConnectionAdded(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionRemovedOverDestroyedRphReturnsFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  // Test
  rph_.reset();
  EXPECT_FALSE(OnPeerConnectionRemoved(key));
}

TEST_F(WebRtcEventLogManagerTest,
       OnPeerConnectionStoppedOverDestroyedRphReturnsFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  // Test
  rph_.reset();
  EXPECT_FALSE(OnPeerConnectionStopped(key));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingOverDestroyedRphReturnsFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  // Test
  rph_.reset();
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureDeadRenderProcessHost);
}

TEST_F(WebRtcEventLogManagerTest,
       OnWebRtcEventLogWriteOverDestroyedRphReturnsFalseAndFalse) {
  // Setup - make sure the |false| returned by the function being tested is
  // related to the RPH being dead, and not due other restrictions.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());

  // Test
  rph_.reset();
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTest, DifferentProfilesCanHaveDifferentPolicies) {
  auto policy_disabled_profile =
      CreateBrowserContext("disabled", true /* is_managed_profile */,
                           false /* has_device_level_policies */,
                           false /* policy_allows_remote_logging */);
  auto policy_disabled_rph =
      std::make_unique<MockRenderProcessHost>(policy_disabled_profile.get());
  const auto disabled_key =
      GetPeerConnectionKey(policy_disabled_rph.get(), kLid);

  auto policy_enabled_profile =
      CreateBrowserContext("enabled", true /* is_managed_profile */,
                           false /* has_device_level_policies */,
                           true /* policy_allows_remote_logging */);
  auto policy_enabled_rph =
      std::make_unique<MockRenderProcessHost>(policy_enabled_profile.get());
  const auto enabled_key = GetPeerConnectionKey(policy_enabled_rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(disabled_key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(disabled_key));

  ASSERT_TRUE(OnPeerConnectionAdded(enabled_key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(enabled_key));

  EXPECT_FALSE(StartRemoteLogging(disabled_key));
  EXPECT_TRUE(StartRemoteLogging(enabled_key));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingWithTooLowWebAppIdRejected) {
  const size_t web_app_id = kMinWebRtcEventLogWebAppId - 1;
  ASSERT_LT(web_app_id, kMinWebRtcEventLogWebAppId);  // Avoid wrap-around.

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_FALSE(StartRemoteLogging(key, GetUniqueId(key),
                                  kMaxRemoteLogFileSizeBytes, 0, web_app_id));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingWithTooHighWebAppIdRejected) {
  const size_t web_app_id = kMaxWebRtcEventLogWebAppId + 1;
  ASSERT_GT(web_app_id, kMaxWebRtcEventLogWebAppId);  // Avoid wrap-around.

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_FALSE(StartRemoteLogging(key, GetUniqueId(key),
                                  kMaxRemoteLogFileSizeBytes, 0, web_app_id));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingWithInRangeWebAppIdAllowedMin) {
  const size_t web_app_id = kMinWebRtcEventLogWebAppId;
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_TRUE(StartRemoteLogging(key, GetUniqueId(key),
                                 kMaxRemoteLogFileSizeBytes, 0, web_app_id));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingWithInRangeWebAppIdAllowedMax) {
  const size_t web_app_id = kMaxWebRtcEventLogWebAppId;
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_TRUE(StartRemoteLogging(key, GetUniqueId(key),
                                 kMaxRemoteLogFileSizeBytes, 0, web_app_id));
}

// Only one remote-bound event log allowed per
TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingOverMultipleWebAppsDisallowed) {
  // Test assumes there are at least two legal web-app IDs.
  ASSERT_NE(kMinWebRtcEventLogWebAppId, kMaxWebRtcEventLogWebAppId);
  const size_t web_app_ids[2] = {kMinWebRtcEventLogWebAppId,
                                 kMaxWebRtcEventLogWebAppId};

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_TRUE(StartRemoteLogging(
      key, GetUniqueId(key), kMaxRemoteLogFileSizeBytes, 0, web_app_ids[0]));
  EXPECT_FALSE(StartRemoteLogging(
      key, GetUniqueId(key), kMaxRemoteLogFileSizeBytes, 0, web_app_ids[1]));
}

TEST_F(WebRtcEventLogManagerTest,
       StartRemoteLoggingWebAppIdIncorporatedIntoFileName) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> file_path;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&file_path)));

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  const size_t expected_web_app_id = kWebAppId;
  ASSERT_TRUE(StartRemoteLogging(key, GetUniqueId(key),
                                 kMaxRemoteLogFileSizeBytes, 0,
                                 expected_web_app_id));
  ASSERT_TRUE(file_path);

  const size_t written_web_app_id =
      ExtractRemoteBoundWebRtcEventLogWebAppIdFromPath(*file_path);
  EXPECT_EQ(written_web_app_id, expected_web_app_id);
}

INSTANTIATE_TEST_SUITE_P(UploadCompleteResult,
                         WebRtcEventLogManagerTest,
                         ::testing::Bool());

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextRemovesPendingFilesInRange) {
  SuppressUploading();

  auto browser_context = CreateBrowserContext("name");
  CreatePendingLogFiles(browser_context.get());
  auto& elements = *(pending_logs_[browser_context.get()]);

  const base::Time earliest_mod = pending_earliest_mod_ - kEpsion;
  const base::Time latest_mod = pending_latest_mod_ + kEpsion;

  // Test - ClearCacheForBrowserContext() removed all of the files in the range.
  ClearCacheForBrowserContext(browser_context.get(), earliest_mod, latest_mod);
  for (size_t i = 0; i < elements.file_paths.size(); ++i) {
    EXPECT_FALSE(base::PathExists(*elements.file_paths[i]));
  }

  ClearPendingLogFiles();
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextCancelsActiveLogFilesIfInRange) {
  SuppressUploading();

  // Setup
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  std::optional<base::FilePath> file_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));

  // Test
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(key)).Times(1);
  ClearCacheForBrowserContext(browser_context_.get(),
                              base::Time::Now() - base::Hours(1),
                              base::Time::Now() + base::Hours(1));
  EXPECT_FALSE(base::PathExists(*file_path));
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextCancelsFileUploadIfInRange) {
  // This factory will enforce the expectation that the upload is cancelled.
  // WebRtcEventLogUploaderImplTest.CancelOnOngoingUploadDeletesFile is in
  // charge of making sure that when the upload is cancelled, the file is
  // removed from disk.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true));

  // Set up and trigger the uploading of a file.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path = CreatePendingRemoteLogFile(key);

  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));
  const base::Time mod_time = GetLastModificationTime(*file_path);

  // Main part of test - the expectation set up in the the uploader factory
  // should now be satisfied.
  ClearCacheForBrowserContext(browser_context_.get(), mod_time - kEpsion,
                              mod_time + kEpsion);
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotRemovePendingFilesOutOfRange) {
  SuppressUploading();

  auto browser_context = CreateBrowserContext("name");
  CreatePendingLogFiles(browser_context.get());
  auto& elements = *(pending_logs_[browser_context.get()]);

  // Get a range whose intersection with the files' range is empty.
  const base::Time earliest_mod = pending_earliest_mod_ - base::Hours(2);
  const base::Time latest_mod = pending_earliest_mod_ - base::Hours(1);
  ASSERT_LT(latest_mod, pending_latest_mod_);

  // Test - ClearCacheForBrowserContext() does not remove files not in range.
  // (Range chosen to be earlier than the oldest file
  ClearCacheForBrowserContext(browser_context.get(), earliest_mod, latest_mod);
  for (size_t i = 0; i < elements.file_paths.size(); ++i) {
    EXPECT_TRUE(base::PathExists(*elements.file_paths[i]));
  }

  ClearPendingLogFiles();
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelActiveLogFilesIfOutOfRange) {
  SuppressUploading();

  // Setup
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  std::optional<base::FilePath> file_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&file_path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));

  // Test
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(_)).Times(0);
  ClearCacheForBrowserContext(browser_context_.get(),
                              base::Time::Now() - base::Hours(2),
                              base::Time::Now() - base::Hours(1));
  EXPECT_TRUE(base::PathExists(*file_path));
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelFileUploadIfOutOfRange) {
  // This factory will enforce the expectation that the upload is not cancelled.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(false));

  // Set up and trigger the uploading of a file.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path = CreatePendingRemoteLogFile(key);

  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));
  const base::Time mod_time = GetLastModificationTime(*file_path);

  // Main part of test - the expectation set up in the the uploader factory,
  // that the upload will not be cancelled, should be shown to hold true.
  // should now be satisfied.
  ClearCacheForBrowserContext(browser_context_.get(), mod_time + kEpsion,
                              mod_time + 2 * kEpsion);
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotRemovePendingFilesFromOtherProfiles) {
  SuppressUploading();

  auto cleared_browser_context = CreateBrowserContext("cleared");
  CreatePendingLogFiles(cleared_browser_context.get());
  auto& cleared_elements = *(pending_logs_[cleared_browser_context.get()]);

  auto const uncleared_browser_context = CreateBrowserContext("pristine");
  CreatePendingLogFiles(uncleared_browser_context.get());
  auto& uncleared_elements = *(pending_logs_[uncleared_browser_context.get()]);

  ASSERT_EQ(cleared_elements.file_paths.size(),
            uncleared_elements.file_paths.size());
  const size_t kFileCount = cleared_elements.file_paths.size();

  const base::Time earliest_mod = pending_earliest_mod_ - kEpsion;
  const base::Time latest_mod = pending_latest_mod_ + kEpsion;

  // Test - ClearCacheForBrowserContext() only removes the files which belong
  // to the cleared context.
  ClearCacheForBrowserContext(cleared_browser_context.get(), earliest_mod,
                              latest_mod);
  for (size_t i = 0; i < kFileCount; ++i) {
    EXPECT_FALSE(base::PathExists(*cleared_elements.file_paths[i]));
    EXPECT_TRUE(base::PathExists(*uncleared_elements.file_paths[i]));
  }

  ClearPendingLogFiles();
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelActiveLogsFromOtherProfiles) {
  SuppressUploading();

  // Remote-bound active log file that *will* be cleared.
  auto cleared_browser_context = CreateBrowserContext("cleared");
  auto cleared_rph =
      std::make_unique<MockRenderProcessHost>(cleared_browser_context.get());
  const auto cleared_key = GetPeerConnectionKey(cleared_rph.get(), kLid);
  std::optional<base::FilePath> cleared_file_path =
      CreateActiveRemoteLogFile(cleared_key);

  // Remote-bound active log file that will *not* be cleared.
  auto uncleared_browser_context = CreateBrowserContext("pristine");
  auto uncleared_rph =
      std::make_unique<MockRenderProcessHost>(uncleared_browser_context.get());
  const auto uncleared_key = GetPeerConnectionKey(uncleared_rph.get(), kLid);
  std::optional<base::FilePath> uncleared_file_path =
      CreateActiveRemoteLogFile(uncleared_key);

  // Test - ClearCacheForBrowserContext() only removes the files which belong
  // to the cleared context.
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(cleared_key)).Times(1);
  EXPECT_CALL(remote_observer_, OnRemoteLogStopped(uncleared_key)).Times(0);
  ClearCacheForBrowserContext(cleared_browser_context.get(), base::Time::Min(),
                              base::Time::Max());
  EXPECT_FALSE(base::PathExists(*cleared_file_path));
  EXPECT_TRUE(base::PathExists(*uncleared_file_path));

  // Cleanup - uncleared_file_path will be closed as part of the shutdown. It
  // is time to clear its expectation.
  testing::Mock::VerifyAndClearExpectations(&remote_observer_);
}

TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotCancelFileUploadFromOtherProfiles) {
  // This factory will enforce the expectation that the upload is not cancelled.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(false));

  // Set up and trigger the uploading of a file.
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> file_path = CreatePendingRemoteLogFile(key);

  ASSERT_TRUE(file_path);
  ASSERT_TRUE(base::PathExists(*file_path));
  const base::Time mod_time = GetLastModificationTime(*file_path);

  // Main part of test - the expectation set up in the the uploader factory,
  // that the upload will not be cancelled, should be shown to hold true.
  // should now be satisfied.
  auto const different_browser_context = CreateBrowserContext();
  ClearCacheForBrowserContext(different_browser_context.get(),
                              mod_time - kEpsion, mod_time + kEpsion);
}

// Show that clearing browser cache, while it removes remote-bound logs, does
// not interfere with local-bound logging, even if that happens on the same PC.
TEST_F(WebRtcEventLogManagerTestCacheClearing,
       ClearCacheForBrowserContextDoesNotInterfereWithLocalLogs) {
  SuppressUploading();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);

  std::optional<base::FilePath> local_log;
  ON_CALL(local_observer_, OnLocalLogStarted(key, _))
      .WillByDefault(Invoke(SaveFilePathTo(&local_log)));
  ASSERT_TRUE(EnableLocalLogging());

  // This adds a peer connection for |key|, which also triggers
  // OnLocalLogStarted() on |local_observer_|.
  auto pending_remote_log = CreatePendingRemoteLogFile(key);

  // Test focus - local logging is uninterrupted.
  EXPECT_CALL(local_observer_, OnLocalLogStopped(_)).Times(0);
  ClearCacheForBrowserContext(browser_context_.get(), base::Time::Min(),
                              base::Time::Max());
  EXPECT_TRUE(base::PathExists(*local_log));

  // Sanity on the test itself; the remote log should have been cleared.
  ASSERT_FALSE(base::PathExists(*pending_remote_log));
}

// When cache clearing cancels the active upload, the next (non-deleted) pending
// file becomes eligible for upload.
TEST_F(WebRtcEventLogManagerTestCacheClearing,
       UploadCancellationTriggersUploadOfNextPendingFile) {
  // The first created file will start being uploaded, but then cancelled.
  // The second file will never be uploaded (deleted while pending).
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true));

  // Create the files that will be deleted when cache is cleared.
  CreatePendingRemoteLogFile(GetPeerConnectionKey(rph_.get(), 0));
  CreatePendingRemoteLogFile(GetPeerConnectionKey(rph_.get(), 1));

  // Create the not-deleted file under a different profile, to easily make sure
  // it does not fit in the ClearCacheForBrowserContext range (less fiddly than
  // a time range).
  auto other_browser_context = CreateBrowserContext();
  auto other_rph =
      std::make_unique<MockRenderProcessHost>(other_browser_context.get());
  const auto key = GetPeerConnectionKey(other_rph.get(), kLid);
  std::optional<base::FilePath> other_file = CreatePendingRemoteLogFile(key);
  ASSERT_TRUE(other_file);

  // Switch the uploader factory to one that will allow us to ensure that the
  // new file, which is not deleted, is uploaded.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {
      WebRtcLogFileInfo(GetBrowserContextId(other_browser_context.get()),
                        *other_file, GetLastModificationTime(*other_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Clearing the cache for the first profile, should now trigger the upload
  // of the last remaining unclear pending log file - |other_file|.
  ClearCacheForBrowserContext(browser_context_.get(), base::Time::Min(),
                              base::Time::Max());
  WaitForPendingTasks(&run_loop);

  // Restore factory before `run_loop` goes out of scope.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true));
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityOnPeerConnectionAdded) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  EXPECT_TRUE(OnPeerConnectionAdded(key));
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityOnPeerConnectionRemoved) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_TRUE(OnPeerConnectionRemoved(key));
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityOnPeerConnectionStopped) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  OnPeerConnectionStopped(key);  // No crash.
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityEnableLocalLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(EnableLocalLogging());
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityDisableLocalLogging) {
  ASSERT_TRUE(EnableLocalLogging());
  EXPECT_TRUE(DisableLocalLogging());
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityStartRemoteLogging) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  std::string error_message;
  EXPECT_FALSE(StartRemoteLogging(key, nullptr, &error_message));
  EXPECT_EQ(error_message, kStartRemoteLoggingFailureFeatureDisabled);
}

TEST_P(WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
       SanityOnWebRtcEventLogWrite) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_FALSE(StartRemoteLogging(key));
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(false, false));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcEventLogManagerTestWithRemoteLoggingDisabled,
                         ::testing::Bool());

// This test is redundant; it is provided for completeness; see following tests.
TEST_F(WebRtcEventLogManagerTestPolicy, StartsEnabledAllowsRemoteLogging) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  const bool allow_remote_logging = true;
  auto browser_context = CreateBrowserContext(
      "name", true /* is_managed_profile */,
      false /* has_device_level_policies */, allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

// This test is redundant; it is provided for completeness; see following tests.
TEST_F(WebRtcEventLogManagerTestPolicy, StartsDisabledRejectsRemoteLogging) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  const bool allow_remote_logging = false;
  auto browser_context = CreateBrowserContext(
      "name", true /* is_managed_profile */,
      false /* has_device_level_policies */, allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

TEST_F(WebRtcEventLogManagerTestPolicy, NotManagedRejectsRemoteLogging) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  const bool allow_remote_logging = false;
  auto browser_context =
      CreateBrowserContext("name", false /* is_managed_profile */,
                           false /* has_device_level_policies */, std::nullopt);

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<user_manager::ScopedUserManager>
WebRtcEventLogManagerTestPolicy::GetScopedUserManager(
    user_manager::UserType user_type) {
  const AccountId kAccountId = AccountId::FromUserEmailGaiaId("name", "id");
  auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
  // On Chrome OS, there are different user types, some of which can be
  // affiliated with the device if the device is enterprise-enrolled, i.e. the
  // logged in account belongs to the org that owns the device. For our
  // purposes here, affiliation does not matter for the determination of the
  // policy default, so we can set it to false here. We do not need a user
  // to profile mapping either, so profile can be a nullptr.
  fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
      kAccountId, /*is_affiliated*/ false, user_type, /*profile*/ nullptr);
  return std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));
}
#endif

TEST_F(WebRtcEventLogManagerTestPolicy,
       ManagedProfileAllowsRemoteLoggingByDefault) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  const bool allow_remote_logging = true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager =
      GetScopedUserManager(user_manager::UserType::kRegular);
#endif

  auto browser_context =
      CreateBrowserContext("name", true /* is_managed_profile */,
                           false /* has_device_level_policies */, std::nullopt);

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

// Currently we only test the case of supervised child profiles for Chrome OS
// here. Other user types for Chrome OS are tested in the unit test for
// ProfileDefaultsToLoggingEnabledTestCase in
// webrtc_event_log_manager_common_unittest because the test setup in this
// class currently does not seem to allow for an easy setup of some user types.
// TODO(crbug.com/1035829): Figure out whether this can be resolved by tweaking
// the test setup or whether the Active Directory services need to be adapted
// for easy testing.
#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebRtcEventLogManagerTestPolicy,
       ManagedProfileDoesNotAllowRemoteLoggingForSupervisedProfiles) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  const bool allow_remote_logging = false;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager =
      GetScopedUserManager(user_manager::UserType::kChild);

  auto browser_context = CreateBrowserContextWithCustomSupervision(
      "name", true /* is_managed_profile */,
      false /* has_device_level_policies */, true /* is_supervised */,
      std::nullopt);

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebRtcEventLogManagerTestPolicy,
       OnlyManagedByPlatformPoliciesDoesNotAllowRemoteLoggingByDefault) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  const bool allow_remote_logging = false;
  auto browser_context =
      CreateBrowserContext("name", false /* is_managed_profile */,
                           true /* has_device_level_policies */, std::nullopt);

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}
#endif

void WebRtcEventLogManagerTestPolicy::TestManagedProfileAfterBeingExplicitlySet(
    bool explicitly_set_value) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  auto profile =
      CreateBrowserContext("name", true /* is_managed_profile */,
                           false /* has_device_level_policies */, std::nullopt);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  explicitly_set_value);
  EXPECT_EQ(StartRemoteLogging(key), explicitly_set_value);
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       ManagedProfileAllowsRemoteLoggingAfterBeingExplicitlyEnabled) {
  TestManagedProfileAfterBeingExplicitlySet(true);
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       ManagedProfileDisallowsRemoteLoggingAfterBeingDisabled) {
  TestManagedProfileAfterBeingExplicitlySet(false);
}

// #1 and #2 differ in the order of AddPeerConnection and the changing of
// the pref value.
TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsEnabledThenDisabledRejectsRemoteLogging1) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  bool allow_remote_logging = true;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

// #1 and #2 differ in the order of AddPeerConnection and the changing of
// the pref value.
TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsEnabledThenDisabledRejectsRemoteLogging2) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  bool allow_remote_logging = true;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

// #1 and #2 differ in the order of AddPeerConnection and the changing of
// the pref value.
TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsDisabledThenEnabledAllowsRemoteLogging1) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  bool allow_remote_logging = false;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

// #1 and #2 differ in the order of AddPeerConnection and the changing of
// the pref value.
TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsDisabledThenEnabledAllowsRemoteLogging2) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  bool allow_remote_logging = false;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  EXPECT_EQ(StartRemoteLogging(key), allow_remote_logging);
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsDisabledThenEnabledUploadsPendingLogFiles) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  bool allow_remote_logging = false;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(allow_remote_logging)
      << "Must turn on before StartRemoteLogging, to test the right thing.";
  ASSERT_EQ(StartRemoteLogging(key), allow_remote_logging);
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsEnabledThenDisabledDoesNotUploadPendingLogFiles) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  SuppressUploading();

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  bool allow_remote_logging = true;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(allow_remote_logging)
      << "Must turn off after StartRemoteLogging, to test the right thing.";
  ASSERT_EQ(StartRemoteLogging(key), allow_remote_logging);
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  UnsuppressUploading();

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsEnabledThenDisabledDeletesPendingLogFiles) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  SuppressUploading();

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  bool allow_remote_logging = true;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(allow_remote_logging)
      << "Must turn off after StartRemoteLogging, to test the right thing.";
  ASSERT_EQ(StartRemoteLogging(key), allow_remote_logging);
  ASSERT_TRUE(log_file);

  // Make the file PENDING.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  ASSERT_TRUE(base::PathExists(*log_file));  // Test sanity; exists before.

  allow_remote_logging = !allow_remote_logging;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  WaitForPendingTasks(&run_loop);

  // Test focus - file deleted without being uploaded.
  EXPECT_FALSE(base::PathExists(*log_file));

  // Still not uploaded.
  UnsuppressUploading();
  WaitForPendingTasks();
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       StartsEnabledThenDisabledCancelsAndDeletesCurrentlyUploadedLogFile) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  // This factory expects exactly one log to be uploaded, then cancelled.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true, 1));

  bool allow_remote_logging = true;
  auto profile = CreateBrowserContext("name", true /* is_managed_profile */,
                                      false /* has_device_level_policies */,
                                      allow_remote_logging);

  auto rph = std::make_unique<MockRenderProcessHost>(profile.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(allow_remote_logging)
      << "Must turn off after StartRemoteLogging, to test the right thing.";
  ASSERT_EQ(StartRemoteLogging(key), allow_remote_logging);
  ASSERT_TRUE(log_file);

  // Log file's upload commences.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  ASSERT_TRUE(base::PathExists(*log_file));  // Test sanity; exists before.

  allow_remote_logging = false;
  profile->GetPrefs()->SetBoolean(prefs::kWebRtcEventLogCollectionAllowed,
                                  allow_remote_logging);

  WaitForPendingTasks();

  // Test focus - file deleted without being uploaded.
  // When the test terminates, the NullWebRtcEventLogUploader::Factory's
  // expectation that one log file was uploaded, and that the upload was
  // cancelled, is enforced.
  // Deletion of the file not performed by NullWebRtcEventLogUploader; instead,
  // WebRtcEventLogUploaderImplTest.CancelOnOngoingUploadDeletesFile tests that.
}

// This test makes sure that if the policy was enabled in the past, but was
// disabled while Chrome was not running, pending logs created during the
// earlier session will be deleted from disk.
TEST_F(WebRtcEventLogManagerTestPolicy,
       PendingLogsFromPreviousSessionRemovedIfPolicyDisabledAtNewSessionStart) {
  SetUp(true);  // Feature generally enabled (kill-switch not engaged).

  SuppressUploading();

  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true, 0));

  bool allow_remote_logging = true;
  auto browser_context = CreateBrowserContext(
      "name", true /* is_managed_profile */,
      false /* has_device_level_policies */, allow_remote_logging);

  const base::FilePath browser_context_dir =
      RemoteBoundLogsDir(browser_context.get());
  ASSERT_TRUE(base::DirectoryExists(browser_context_dir));

  auto rph = std::make_unique<MockRenderProcessHost>(browser_context.get());
  const auto key = GetPeerConnectionKey(rph.get(), kLid);

  // Produce an empty log file in the BrowserContext. It's not uploaded
  // because uploading is suppressed.
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(allow_remote_logging)
      << "Must turn off after StartRemoteLogging, to test the right thing.";
  ASSERT_EQ(StartRemoteLogging(key), allow_remote_logging);
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  // Reload the BrowserContext, but this time with the policy disabling
  // the feature.
  rph.reset();
  browser_context.reset();
  ASSERT_TRUE(base::DirectoryExists(browser_context_dir));  // Test sanity
  allow_remote_logging = false;
  browser_context = CreateBrowserContext("name", true /* is_managed_profile */,
                                         false /* has_device_level_policies */,
                                         allow_remote_logging);

  // Test focus - pending log files removed, as well as any potential metadata
  // associated with remote-bound logging for |browser_context|.
  ASSERT_FALSE(base::DirectoryExists(browser_context_dir));

  // When NullWebRtcEventLogUploader::Factory is destroyed, it will show that
  // the deleted log file was never uploaded.
  UnsuppressUploading();
  WaitForPendingTasks();
}

TEST_F(WebRtcEventLogManagerTestPolicy,
       PendingLogsFromPreviousSessionRemovedIfRemoteLoggingKillSwitchEngaged) {
  SetUp(false);  // Feature generally disabled (kill-switch engaged).

  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true, 0));

  const std::string name = "name";
  const base::FilePath browser_context_dir =
      profiles_dir_.GetPath().AppendASCII(name);
  const base::FilePath remote_bound_dir =
      RemoteBoundLogsDir(browser_context_dir);
  ASSERT_FALSE(base::PathExists(remote_bound_dir));

  base::FilePath file_path;
  base::File file;
  ASSERT_TRUE(base::CreateDirectory(remote_bound_dir));
  ASSERT_TRUE(CreateRemoteBoundLogFile(remote_bound_dir, kWebAppId,
                                       remote_log_extension_, base::Time::Now(),
                                       &file_path, &file));
  file.Close();

  const bool allow_remote_logging = true;
  auto browser_context = CreateBrowserContext(
      "name", true /* is_managed_profile */,
      false /* has_device_level_policies */, allow_remote_logging);
  ASSERT_EQ(browser_context->GetPath(), browser_context_dir);  // Test sanity

  WaitForPendingTasks();

  EXPECT_FALSE(base::PathExists(remote_bound_dir));
}

TEST_F(WebRtcEventLogManagerTestUploadSuppressionDisablingFlag,
       UploadingNotSuppressedByActivePeerConnections) {
  SuppressUploading();

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       DoNotUploadPendingLogsIfConnectedToUnsupportedNetworkType) {
  SetUpNetworkConnection(get_conn_type_is_sync_, unsupported_type_);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  std::list<WebRtcLogFileInfo> empty_expected_files_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_expected_files_list, true, &run_loop));

  // Peer connection removal MAY trigger upload, depending on network.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       UploadPendingLogsIfConnectedToSupportedNetworkType) {
  SetUpNetworkConnection(get_conn_type_is_sync_, supported_type_);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Peer connection removal MAY trigger upload, depending on network.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       UploadPendingLogsIfConnectionTypeChangesFromUnsupportedToSupported) {
  SetUpNetworkConnection(get_conn_type_is_sync_, unsupported_type_);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  // That a peer connection upload is not initiated by this point, is verified
  // by previous tests.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));
  WaitForPendingTasks();

  // Test focus - an upload will be initiated after changing the network type.
  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));
  SetConnectionType(supported_type_);

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       DoNotUploadPendingLogsAtStartupIfConnectedToUnsupportedNetworkType) {
  SetUpNetworkConnection(get_conn_type_is_sync_, unsupported_type_);

  UnloadProfileAndSeedPendingLog();

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  std::list<WebRtcLogFileInfo> empty_expected_files_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_expected_files_list, true, &run_loop));

  LoadMainTestProfile();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path_);

  WaitForPendingTasks(&run_loop);
}

TEST_P(WebRtcEventLogManagerTestForNetworkConnectivity,
       UploadPendingLogsAtStartupIfConnectedToSupportedNetworkType) {
  SetUpNetworkConnection(get_conn_type_is_sync_, supported_type_);

  UnloadProfileAndSeedPendingLog();

  // This factory enforces the expectation that the files will be uploaded,
  // all of them, only them, and in the order expected.
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files_, true, &run_loop));

  LoadMainTestProfile();
  ASSERT_EQ(browser_context_->GetPath(), browser_context_path_);

  WaitForPendingTasks(&run_loop);
}

INSTANTIATE_TEST_SUITE_P(
    UploadSupportingConnectionTypes,
    WebRtcEventLogManagerTestForNetworkConnectivity,
    ::testing::Combine(
        // Wehther GetConnectionType() responds synchronously.
        ::testing::Bool(),
        // The upload-supporting network type to be used.
        ::testing::Values(network::mojom::ConnectionType::CONNECTION_ETHERNET,
                          network::mojom::ConnectionType::CONNECTION_WIFI,
                          network::mojom::ConnectionType::CONNECTION_UNKNOWN),
        // The upload-unsupporting network type to be used.
        ::testing::Values(network::mojom::ConnectionType::CONNECTION_NONE,
                          network::mojom::ConnectionType::CONNECTION_4G)));

TEST_F(WebRtcEventLogManagerTestUploadDelay, DoNotInitiateUploadBeforeDelay) {
  SetUp(kIntentionallyExcessiveDelayMs);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  // Wait a bit and see that the upload was not initiated. (Due to technical
  // constraints, we cannot wait forever.)
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  event.TimedWait(base::Milliseconds(500));

  WaitForPendingTasks(&run_loop);
}

// WhenOnPeerConnectionRemovedFinishedRemoteLogUploadedAndFileDeleted has some
// overlap with this, but we still include this test for explicitness and
// clarity.
TEST_F(WebRtcEventLogManagerTestUploadDelay, InitiateUploadAfterDelay) {
  SetUp(kDefaultUploadDelayMs);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_file);

  base::RunLoop run_loop;
  std::list<WebRtcLogFileInfo> expected_files = {WebRtcLogFileInfo(
      browser_context_id_, *log_file, GetLastModificationTime(*log_file))};
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &expected_files, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestUploadDelay,
       OnPeerConnectionAddedDuringDelaySuppressesUpload) {
  SetUp(kIntentionallyExcessiveDelayMs);

  const auto key1 = GetPeerConnectionKey(rph_.get(), 1);
  const auto key2 = GetPeerConnectionKey(rph_.get(), 2);

  ASSERT_TRUE(OnPeerConnectionAdded(key1));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key1));
  ASSERT_TRUE(StartRemoteLogging(key1));

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(OnPeerConnectionRemoved(key1));

  // Test focus - after adding a peer connection, the conditions for the upload
  // are no longer considered to hold.
  // (Test implemented with a glimpse into the black box due to technical
  // limitations and the desire to avoid flakiness.)
  ASSERT_TRUE(OnPeerConnectionAdded(key2));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key2));
  EXPECT_FALSE(UploadConditionsHold());

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestUploadDelay,
       ClearCacheForBrowserContextDuringDelayCancelsItsUpload) {
  SetUp(kIntentionallyExcessiveDelayMs);

  const auto key = GetPeerConnectionKey(rph_.get(), 1);

  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key));

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Change log file from ACTIVE to PENDING.
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  // Test focus - after clearing browser cache, the conditions for the upload
  // are no longer considered to hold, because the file about to be uploaded
  // was deleted.
  // (Test implemented with a glimpse into the black box due to technical
  // limitations and the desire to avoid flakiness.)
  ClearCacheForBrowserContext(browser_context_.get(), base::Time::Min(),
                              base::Time::Max());
  EXPECT_FALSE(UploadConditionsHold());

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestCompression,
       ErroredFilesDueToBadEstimationDeletedRatherThanUploaded) {
  Init(Compression::GZIP_NULL_ESTIMATION);

  const std::string log = "It's better than bad; it's good.";

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  std::optional<base::FilePath> log_file;
  ON_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .WillByDefault(Invoke(SaveFilePathTo(&log_file)));
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_TRUE(StartRemoteLogging(key, GetUniqueId(key), GzippedSize(log) - 1, 0,
                                 kWebAppId));
  ASSERT_TRUE(log_file);

  std::list<WebRtcLogFileInfo> empty_list;
  base::RunLoop run_loop;
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<FileListExpectingWebRtcEventLogUploader::Factory>(
          &empty_list, true, &run_loop));

  // Writing fails because the budget is exceeded.
  EXPECT_EQ(OnWebRtcEventLogWrite(key, log), std::make_pair(false, false));

  // The file was deleted due to the error we've instigated (by using an
  // intentionally over-optimistic estimation).
  EXPECT_FALSE(base::PathExists(*log_file));

  // If the file is incorrectly still eligible for an upload, this will trigger
  // the upload (which will be a test failure).
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  WaitForPendingTasks(&run_loop);
}

TEST_F(WebRtcEventLogManagerTestIncognito, StartRemoteLoggingFails) {
  const auto key = GetPeerConnectionKey(incognito_rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_FALSE(StartRemoteLogging(key));
}

TEST_F(WebRtcEventLogManagerTestIncognito,
       StartRemoteLoggingDoesNotCreateDirectoryOrFiles) {
  const auto key = GetPeerConnectionKey(incognito_rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_FALSE(StartRemoteLogging(key));

  const base::FilePath remote_logs_path =
      RemoteBoundLogsDir(incognito_profile_);
  EXPECT_TRUE(base::IsDirectoryEmpty(remote_logs_path));
}

TEST_F(WebRtcEventLogManagerTestIncognito,
       OnWebRtcEventLogWriteReturnsFalseForRemotePart) {
  const auto key = GetPeerConnectionKey(incognito_rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  ASSERT_FALSE(StartRemoteLogging(key));
  EXPECT_EQ(OnWebRtcEventLogWrite(key, "log"), std::make_pair(false, false));
}

TEST_F(WebRtcEventLogManagerTestHistory,
       CorrectHistoryReturnedForActivelyWrittenLog) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(path);
  ASSERT_FALSE(path->BaseName().MaybeAsASCII().empty());

  const auto history = GetHistory(browser_context_id_);
  ASSERT_EQ(history.size(), 1u);
  const auto history_entry = history[0];

  EXPECT_EQ(history_entry.state, UploadList::UploadInfo::State::Pending);
  EXPECT_TRUE(IsSmallTimeDelta(history_entry.capture_time, base::Time::Now()));
  EXPECT_EQ(history_entry.local_id, path->BaseName().MaybeAsASCII());
  EXPECT_TRUE(history_entry.upload_id.empty());
  EXPECT_TRUE(history_entry.upload_time.is_null());
}

TEST_F(WebRtcEventLogManagerTestHistory, CorrectHistoryReturnedForPendingLog) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(path);
  ASSERT_FALSE(path->BaseName().MaybeAsASCII().empty());

  SuppressUploading();
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  const auto history = GetHistory(browser_context_id_);
  ASSERT_EQ(history.size(), 1u);
  const auto history_entry = history[0];

  EXPECT_EQ(history_entry.state, UploadList::UploadInfo::State::Pending);
  EXPECT_TRUE(IsSmallTimeDelta(history_entry.capture_time, base::Time::Now()));
  EXPECT_EQ(history_entry.local_id, path->BaseName().MaybeAsASCII());
  EXPECT_TRUE(history_entry.upload_id.empty());
  EXPECT_TRUE(history_entry.upload_time.is_null());
}

TEST_F(WebRtcEventLogManagerTestHistory,
       CorrectHistoryReturnedForActivelyUploadedLog) {
  // This factory expects exactly one log to be uploaded; cancellation is
  // expected during tear-down.
  SetWebRtcEventLogUploaderFactoryForTesting(
      std::make_unique<NullWebRtcEventLogUploader::Factory>(true, 1));

  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(path);
  ASSERT_FALSE(path->BaseName().MaybeAsASCII().empty());

  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  const auto history = GetHistory(browser_context_id_);
  ASSERT_EQ(history.size(), 1u);
  const auto history_entry = history[0];

  EXPECT_EQ(history_entry.state, UploadList::UploadInfo::State::Pending);
  EXPECT_TRUE(IsSmallTimeDelta(history_entry.capture_time, base::Time::Now()));
  EXPECT_EQ(history_entry.local_id, path->BaseName().MaybeAsASCII());
  EXPECT_TRUE(history_entry.upload_id.empty());
  EXPECT_TRUE(IsSmallTimeDelta(history_entry.upload_time, base::Time::Now()));
  EXPECT_LE(history_entry.capture_time, history_entry.upload_time);

  // Test tear down - trigger uploader cancellation.
  ClearCacheForBrowserContext(browser_context_.get(), base::Time::Min(),
                              base::Time::Max());
}

// See ExpiredLogFilesAreReplacedByHistoryFiles for verification of the
// creation of history files of this type.
TEST_F(WebRtcEventLogManagerTestHistory,
       ExpiredLogFilesReplacedByHistoryFilesAndGetHistoryReportsAccordingly) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> log_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&log_path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_path);
  ASSERT_FALSE(log_path->BaseName().MaybeAsASCII().empty());

  SuppressUploading();
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  UnloadMainTestProfile();

  // Test sanity.
  ASSERT_TRUE(base::PathExists(*log_path));

  // Pretend more time than kRemoteBoundWebRtcEventLogsMaxRetention has passed.
  const base::TimeDelta elapsed_time =
      kRemoteBoundWebRtcEventLogsMaxRetention + base::Hours(1);
  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(*log_path, &file_info));

  const auto modified_capture_time = file_info.last_modified - elapsed_time;
  ASSERT_TRUE(base::TouchFile(*log_path, file_info.last_accessed - elapsed_time,
                              modified_capture_time));

  LoadMainTestProfile();

  ASSERT_FALSE(base::PathExists(*log_path));

  const auto history = GetHistory(browser_context_id_);
  ASSERT_EQ(history.size(), 1u);
  const auto history_entry = history[0];

  EXPECT_EQ(history_entry.state, UploadList::UploadInfo::State::NotUploaded);
  EXPECT_TRUE(IsSameTimeWhenTruncatedToSeconds(history_entry.capture_time,
                                               modified_capture_time));
  EXPECT_EQ(history_entry.local_id,
            ExtractRemoteBoundWebRtcEventLogLocalIdFromPath(*log_path));
  EXPECT_TRUE(history_entry.upload_id.empty());
  EXPECT_TRUE(history_entry.upload_time.is_null());
}

// Since the uploader mocks do not write the history files, it is not easy
// to check that the correct result is returned for GetHistory() for either
// a successful or an unsuccessful upload from the WebRtcEventLogManager level.
// Instead, this is checked by WebRtcEventLogUploaderImplTest.
// TODO(crbug.com/40545136): Add the tests mention in the comment above.

TEST_F(WebRtcEventLogManagerTestHistory, ClearingCacheRemovesHistoryFiles) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> log_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&log_path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_path);
  ASSERT_FALSE(log_path->BaseName().MaybeAsASCII().empty());

  SuppressUploading();
  ASSERT_TRUE(OnPeerConnectionRemoved(key));

  UnloadMainTestProfile();

  // Test sanity.
  ASSERT_TRUE(base::PathExists(*log_path));

  // Pretend more time than kRemoteBoundWebRtcEventLogsMaxRetention has passed.
  const base::TimeDelta elapsed_time =
      kRemoteBoundWebRtcEventLogsMaxRetention + base::Hours(1);
  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(*log_path, &file_info));

  const auto modified_capture_time = file_info.last_modified - elapsed_time;
  ASSERT_TRUE(base::TouchFile(*log_path, file_info.last_accessed - elapsed_time,
                              modified_capture_time));

  LoadMainTestProfile();

  ASSERT_FALSE(base::PathExists(*log_path));

  // Setup complete; we now have a history file on disk. Time to see that it is
  // removed when cache is cleared.

  // Sanity.
  const auto history_path = GetWebRtcEventLogHistoryFilePath(*log_path);
  ASSERT_TRUE(base::PathExists(history_path));
  ASSERT_EQ(GetHistory(browser_context_id_).size(), 1u);

  // Test.
  ClearCacheForBrowserContext(browser_context_.get(), base::Time::Min(),
                              base::Time::Max());
  ASSERT_FALSE(base::PathExists(history_path));
  ASSERT_EQ(GetHistory(browser_context_id_).size(), 0u);
}

TEST_F(WebRtcEventLogManagerTestHistory,
       ClearingCacheDoesNotLeaveBehindHistoryForRemovedLogs) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));

  std::optional<base::FilePath> log_path;
  EXPECT_CALL(remote_observer_, OnRemoteLogStarted(key, _, _))
      .Times(1)
      .WillOnce(Invoke(SaveFilePathTo(&log_path)));
  ASSERT_TRUE(StartRemoteLogging(key));
  ASSERT_TRUE(log_path);
  ASSERT_FALSE(log_path->BaseName().MaybeAsASCII().empty());

  ASSERT_TRUE(base::PathExists(*log_path));
  ClearCacheForBrowserContext(browser_context_.get(), base::Time::Min(),
                              base::Time::Max());
  ASSERT_FALSE(base::PathExists(*log_path));

  const auto history = GetHistory(browser_context_id_);
  EXPECT_EQ(history.size(), 0u);
}

// TODO(crbug.com/40545136): Add a test for the limit on the number of history
// files allowed to remain on disk.

#else  // BUILDFLAG(IS_ANDROID)

class WebRtcEventLogManagerTestOnMobileDevices
    : public WebRtcEventLogManagerTestBase {
 public:
  WebRtcEventLogManagerTestOnMobileDevices() {
    // features::kWebRtcRemoteEventLog not defined on mobile, and can therefore
    // not be forced on. This test is here to make sure that when the feature
    // is changed to be on by default, it will still be off for mobile devices.
    CreateWebRtcEventLogManager();
  }
};

TEST_F(WebRtcEventLogManagerTestOnMobileDevices, RemoteBoundLoggingDisabled) {
  const auto key = GetPeerConnectionKey(rph_.get(), kLid);
  ASSERT_TRUE(OnPeerConnectionAdded(key));
  ASSERT_TRUE(OnPeerConnectionSessionIdSet(key));
  EXPECT_FALSE(StartRemoteLogging(key));
}

#endif

}  // namespace webrtc_event_logging
