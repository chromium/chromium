// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/downloads/downloads_api.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/circular_deque.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_file_icon_extractor.h"
#include "chrome/browser/download/download_open_prompt.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/extensions/api/downloads_internal/downloads_internal_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/platform_util_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/common/extensions/api/downloads.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/test_download_http_response.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "net/base/data_url.h"
#include "net/base/mime_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using download::DownloadItem;

namespace errors = download_extension_errors;

namespace extensions {
namespace downloads = api::downloads;

namespace {

const char kFirstDownloadUrl[] = "/download1";
const char kSecondDownloadUrl[] = "/download2";
const int kDownloadSize = 1024 * 10;

void OnFileDeleted(bool success) {}

// Comparator that orders download items by their ID. Can be used with
// std::sort.
struct DownloadIdComparator {
  bool operator() (DownloadItem* first, DownloadItem* second) {
    return first->GetId() < second->GetId();
  }
};

bool IsDownloadExternallyRemoved(download::DownloadItem* item) {
  return item->GetFileExternallyRemoved();
}

void OnOpenPromptCreated(download::DownloadItem* item,
                         DownloadOpenPrompt* prompt) {
  EXPECT_FALSE(item->GetOpened());
  // Posts a task to accept the DownloadOpenPrompt.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadOpenPrompt::AcceptConfirmationDialogForTesting,
                     base::Unretained(prompt)));
}

class DownloadsEventsListener : public EventRouter::TestObserver {
 public:
  explicit DownloadsEventsListener(Profile* profile)
      : waiting_(false), profile_(profile) {}

  DownloadsEventsListener(const DownloadsEventsListener&) = delete;
  DownloadsEventsListener& operator=(const DownloadsEventsListener&) = delete;

  ~DownloadsEventsListener() override = default;

  void ClearEvents() { events_.clear(); }

  class Event {
   public:
    Event(Profile* profile,
          const std::string& event_name,
          const base::Value& args,
          base::Time caught)
        : profile_(profile),
          event_name_(event_name),
          args_(args.Clone()),
          caught_(caught) {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    const base::Time& caught() { return caught_; }

    bool Satisfies(const Event& other) const {
      return other.SatisfiedBy(*this);
    }

    bool SatisfiedBy(const Event& other) const {
      // Only match profile iff restrict_to_browser_context is non-null when
      // the event is dispatched from
      // ExtensionDownloadsEventRouter::DispatchEvent. Event names always have
      // to match.
      if ((profile_ && other.profile_ && profile_ != other.profile_) ||
          (event_name_ != other.event_name_))
        return false;

      if (((event_name_ == downloads::OnDeterminingFilename::kEventName) ||
           (event_name_ == downloads::OnCreated::kEventName) ||
           (event_name_ == downloads::OnChanged::kEventName))) {
        // We expect a non-empty list for these events.
        if (!args_.is_list() || !other.args_.is_list() ||
            args_.GetList().empty() || other.args_.GetList().empty())
          return false;
        const base::Value& left_value = args_.GetList()[0];
        const base::Value& right_value = other.args_.GetList()[0];
        if (!left_value.is_dict() || !right_value.is_dict()) {
          return false;
        }

        const base::Value::Dict& left_dict = left_value.GetDict();
        const base::Value::Dict& right_dict = right_value.GetDict();
        // Expect that all keys present in both dictionaries are equal. If a key
        // is only present in one of the dictionaries, ignore it. This allows us
        // to verify the properties we care about in the test without needing to
        // specify each.
        for (const auto [left_dict_key, left_dict_value] : left_dict) {
          const base::Value* right_dict_value = right_dict.Find(left_dict_key);
          if (!right_dict_value || *right_dict_value != left_dict_value) {
            return false;
          }
        }
        return true;
      }
      return args_ == other.args_;
    }

    std::string Debug() {
      return base::StringPrintf("Event(%p, %s, %f)", profile_.get(),
                                event_name_.c_str(),
                                caught_.InMillisecondsFSinceUnixEpoch());
    }

   private:
    raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;
    std::string event_name_;
    std::string json_args_;
    base::Value args_;
    base::Time caught_;
  };

  // extensions::EventRouter::TestObserver:
  void OnWillDispatchEvent(const extensions::Event& event) override {
    // TestObserver receives notifications for all events but only needs to
    // check download events.
    if (!base::StartsWith(event.event_name, "downloads"))
      return;

    Event* new_event = new Event(
        Profile::FromBrowserContext(event.restrict_to_browser_context),
        event.event_name, base::Value(event.event_args.Clone()),
        base::Time::Now());
    events_.push_back(base::WrapUnique(new_event));
    if (waiting_ && waiting_for_.get() && new_event->Satisfies(*waiting_for_)) {
      waiting_ = false;
      std::move(quit_closure_).Run();
    }
  }

  // extensions::EventRouter::TestObserver:
  void OnDidDispatchEventToProcess(const extensions::Event& event,
                                   int process_id) override {}

  bool WaitFor(Profile* profile,
               const std::string& event_name,
               const std::string& json_args) {
    base::RunLoop loop;
    base::Value args = base::JSONReader::Read(json_args).value();
    waiting_for_ =
        std::make_unique<Event>(profile, event_name, args, base::Time());
    for (const auto& event : events_) {
      if (event->Satisfies(*waiting_for_))
        return true;
    }
    waiting_ = true;
    quit_closure_ = loop.QuitWhenIdleClosure();
    loop.Run();
    bool success = !waiting_;
    if (waiting_) {
      // Print the events that were caught since the last WaitFor() call to help
      // find the erroneous event.
      // TODO(benjhayden) Fuzzy-match and highlight the erroneous event.
      for (const auto& event : events_) {
        if (event->caught() > last_wait_) {
          LOG(INFO) << "Caught " << event->Debug();
        }
      }
      if (waiting_for_.get()) {
        LOG(INFO) << "Timed out waiting for " << waiting_for_->Debug();
      }
      waiting_ = false;
    }
    waiting_for_.reset();
    last_wait_ = base::Time::Now();
    return success;
  }

  void UpdateProfile(Profile* profile) { profile_ = profile; }

  base::circular_deque<std::unique_ptr<Event>>* events() { return &events_; }

 private:
  bool waiting_;
  base::Time last_wait_;
  std::unique_ptr<Event> waiting_for_;
  base::circular_deque<std::unique_ptr<Event>> events_;
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;
  base::OnceClosure quit_closure_;
};

// Object waiting for a download open event.
class DownloadOpenObserver : public download::DownloadItem::Observer {
 public:
  explicit DownloadOpenObserver(download::DownloadItem* item) : item_(item) {
    open_observation_.Observe(item);
  }

  DownloadOpenObserver(const DownloadOpenObserver&) = delete;
  DownloadOpenObserver& operator=(const DownloadOpenObserver&) = delete;

  ~DownloadOpenObserver() override = default;

  void WaitForEvent() {
    if (item_ && !item_->GetOpened()) {
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 private:
  // download::DownloadItem::Observer
  void OnDownloadOpened(download::DownloadItem* item) override {
    if (completion_closure_) {
      std::move(completion_closure_).Run();
    }
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    DCHECK(open_observation_.IsObservingSource(item));
    open_observation_.Reset();
    item_ = nullptr;
  }

  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      open_observation_{this};
  raw_ptr<download::DownloadItem, DanglingUntriaged> item_;
  base::OnceClosure completion_closure_;
};

}  // namespace

class DownloadExtensionTest : public ExtensionApiTest {
 public:
  DownloadExtensionTest()
      : extension_(nullptr),
        incognito_browser_(nullptr),
        current_browser_(nullptr) {}

  DownloadExtensionTest(const DownloadExtensionTest&) = delete;
  DownloadExtensionTest& operator=(const DownloadExtensionTest&) = delete;

 protected:
  // Used with CreateHistoryDownloads
  struct HistoryDownloadInfo {
    // Filename to use. CreateHistoryDownloads will append this filename to the
    // temporary downloads directory specified by downloads_directory().
    const base::FilePath::CharType*   filename;

    // State for the download. Note that IN_PROGRESS downloads will be created
    // as CANCELLED.
    DownloadItem::DownloadState state;

    // Danger type for the download. Only use DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS
    // and DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT.
    download::DownloadDangerType danger_type;
  };

  void LoadExtension(const char* name, bool enable_file_access = false) {
    extension_ = LoadExtensionInternal(name, enable_file_access);
  }

  void LoadSecondExtension(const char* name) {
    second_extension_ = LoadExtensionInternal(name, false);
  }

  content::RenderProcessHost* AddFilenameDeterminer() {
    ExtensionDownloadsEventRouter::SetDetermineFilenameTimeoutSecondsForTesting(
        2);
    content::WebContents* tab = chrome::AddSelectedTabWithURL(
        current_browser(),
        extension_->GetResourceURL("empty.html"),
        ui::PAGE_TRANSITION_LINK);
    EventRouter::Get(current_browser()->profile())
        ->AddEventListener(downloads::OnDeterminingFilename::kEventName,
                           tab->GetPrimaryMainFrame()->GetProcess(),
                           GetExtensionId());
    return tab->GetPrimaryMainFrame()->GetProcess();
  }

  void RemoveFilenameDeterminer(content::RenderProcessHost* host) {
    EventRouter::Get(current_browser()->profile())->RemoveEventListener(
        downloads::OnDeterminingFilename::kEventName, host, GetExtensionId());
  }

  Browser* current_browser() { return current_browser_; }

  // InProcessBrowserTest
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    GoOnTheRecord();
    current_browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPromptForDownload, false);
    // Create event listener using current profile.
    events_listener_ =
        std::make_unique<DownloadsEventsListener>(current_browser()->profile());
    extensions::EventRouter::Get(current_browser()->profile())
        ->AddObserverForTesting(events_listener());
    // Disable file chooser for current profile.
    DownloadTestFileActivityObserver observer(current_browser()->profile());
    observer.EnableFileChooser(false);

    first_download_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kFirstDownloadUrl);
    second_download_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kSecondDownloadUrl);

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // InProcessBrowserTest
  void TearDownOnMainThread() override {
    EventRouter::Get(current_browser()->profile())
        ->RemoveObserverForTesting(events_listener_.get());
    ExtensionApiTest::TearDownOnMainThread();
  }

  void GoOnTheRecord() {
    current_browser_ = browser();
    if (events_listener_.get())
      events_listener_->UpdateProfile(current_browser()->profile());
  }

  void GoOffTheRecord() {
    if (!incognito_browser_) {
      incognito_browser_ = CreateIncognitoBrowser();
      // Disable file chooser for incognito profile.
      DownloadTestFileActivityObserver observer(incognito_browser_->profile());
      observer.EnableFileChooser(false);
    }
    SetPromptForDownload(incognito_browser_, false);
    current_browser_ = incognito_browser_;
    if (events_listener_.get())
      events_listener_->UpdateProfile(current_browser()->profile());
  }

  bool WaitFor(const std::string& event_name, const std::string& json_args) {
    return events_listener_->WaitFor(
        current_browser()->profile(), event_name, json_args);
  }

  bool WaitForInterruption(DownloadItem* item,
                           download::DownloadInterruptReason expected_error,
                           const std::string& on_created_event) {
    if (!WaitFor(downloads::OnCreated::kEventName, on_created_event))
      return false;
    // Now, onCreated is always fired before interruption.
    return WaitFor(
        downloads::OnChanged::kEventName,
        base::StringPrintf(
            "[{\"id\": %d,"
            "  \"error\": {\"current\": \"%s\"},"
            "  \"state\": {"
            "    \"previous\": \"in_progress\","
            "    \"current\": \"interrupted\"}}]",
            item->GetId(),
            download::DownloadInterruptReasonToString(expected_error).c_str()));
  }

  void ClearEvents() {
    events_listener_->ClearEvents();
  }

  std::string GetExtensionURL() {
    return extension_->url().spec();
  }
  content::StoragePartitionConfig GetExtensionStoragePartitionConfig() {
    return browser()
        ->profile()
        ->GetDownloadManager()
        ->GetStoragePartitionConfigForSiteUrl(extension_->url());
  }
  std::string GetExtensionId() {
    return extension_->id();
  }
  std::string GetSecondExtensionId() { return second_extension_->id(); }

  std::string GetFilename(const char* path) {
    std::string result = downloads_directory().AppendASCII(path).AsUTF8Unsafe();
#if BUILDFLAG(IS_WIN)
    for (std::string::size_type next = result.find("\\");
         next != std::string::npos;
         next = result.find("\\", next)) {
      result.replace(next, 1, "\\\\");
      next += 2;
    }
#endif
    return result;
  }

  DownloadManager* GetOnRecordManager() {
    return browser()->profile()->GetDownloadManager();
  }
  DownloadManager* GetOffRecordManager() {
    return browser()
        ->profile()
        ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
        ->GetDownloadManager();
  }
  DownloadManager* GetCurrentManager() {
    return (current_browser_ == incognito_browser_) ?
      GetOffRecordManager() : GetOnRecordManager();
  }

  // Creates a set of history downloads based on the provided |history_info|
  // array. |count| is the number of elements in |history_info|. On success,
  // |items| will contain |count| DownloadItems in the order that they were
  // specified in |history_info|. Returns true on success and false otherwise.
  bool CreateHistoryDownloads(const HistoryDownloadInfo* history_info,
                              size_t count,
                              DownloadManager::DownloadVector* items) {
    DownloadIdComparator download_id_comparator;
    base::Time current = base::Time::Now();
    items->clear();
    GetOnRecordManager()->GetAllDownloads(items);
    CHECK(items->empty());
    std::vector<GURL> url_chain;
    url_chain.push_back(GURL());
    for (size_t i = 0; i < count; ++i) {
      DownloadItem* item = GetOnRecordManager()->CreateDownloadItem(
          base::Uuid::GenerateRandomV4().AsLowercaseString(),
          download::DownloadItem::kInvalidId + 1 + i,
          downloads_directory().Append(history_info[i].filename),
          downloads_directory().Append(history_info[i].filename), url_chain,
          GURL(),
          content::StoragePartitionConfig::CreateDefault(browser()->profile()),
          GURL(), GURL(), url::Origin(), std::string(),
          std::string(),  // mime_type, original_mime_type
          current,
          current,  // start_time, end_time
          std::string(),
          std::string(),  // etag, last_modified
          1,
          1,                      // received_bytes, total_bytes
          std::string(),          // hash
          history_info[i].state,  // state
          history_info[i].danger_type,
          (history_info[i].state != download::DownloadItem::CANCELLED
               ? download::DOWNLOAD_INTERRUPT_REASON_NONE
               : download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED),
          false,    // opened
          current,  // last_access_time
          false, std::vector<DownloadItem::ReceivedSlice>());
      items->push_back(item);
    }

    // Order by ID so that they are in the order that we created them.
    std::sort(items->begin(), items->end(), download_id_comparator);
    return true;
  }

  void CreateTwoDownloads(DownloadManager::DownloadVector* items) {
    CreateFirstSlowTestDownload();
    CreateSecondSlowTestDownload();

    GetCurrentManager()->GetAllDownloads(items);
    ASSERT_EQ(2u, items->size());
  }

  DownloadItem* CreateFirstSlowTestDownload() {
    DownloadManager* manager = GetCurrentManager();

    EXPECT_EQ(0, manager->BlockingShutdownCount());
    EXPECT_EQ(0, manager->InProgressCount());
    if (manager->InProgressCount() != 0)
      return nullptr;
    return CreateSlowTestDownload(first_download_.get(), kFirstDownloadUrl);
  }

  DownloadItem* CreateSecondSlowTestDownload() {
    return CreateSlowTestDownload(second_download_.get(), kSecondDownloadUrl);
  }

  DownloadItem* CreateSlowTestDownload(
      net::test_server::ControllableHttpResponse* response,
      const std::string& path) {
    if (!embedded_test_server()->Started())
      StartEmbeddedTestServer();
    std::unique_ptr<content::DownloadTestObserver> observer(
        CreateInProgressDownloadObserver(1));
    DownloadManager* manager = GetCurrentManager();

    const GURL url = embedded_test_server()->GetURL(path);
    ui_test_utils::NavigateToURLWithDisposition(
        current_browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);

    response->WaitForRequest();
    response->Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-type: application/octet-stream\r\n"
        "Cache-Control: max-age=0\r\n"
        "\r\n");
    response->Send(std::string(kDownloadSize, '*'));

    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::IN_PROGRESS));

    DownloadManager::DownloadVector items;
    manager->GetAllDownloads(&items);
    EXPECT_TRUE(!items.empty());
    return items.back();
  }

  void FinishFirstSlowDownloads() {
    FinishSlowDownloads(first_download_.get());
  }

  void FinishSecondSlowDownloads() {
    FinishSlowDownloads(second_download_.get());
  }

  void FinishSlowDownloads(
      net::test_server::ControllableHttpResponse* response) {
    std::unique_ptr<content::DownloadTestObserver> observer(
        CreateDownloadObserver(1));
    response->Done();
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  }

  content::DownloadTestObserver* CreateDownloadObserver(size_t download_count) {
    return new content::DownloadTestObserverTerminal(
        GetCurrentManager(), download_count,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  content::DownloadTestObserver* CreateInProgressDownloadObserver(
      size_t download_count) {
    return new content::DownloadTestObserverInProgress(
        GetCurrentManager(), download_count);
  }

  bool RunFunction(scoped_refptr<ExtensionFunction> function,
                   const std::string& args) {
    return RunFunctionInternal(extension_, function, args);
  }

  bool RunFunctionInSecondExtension(scoped_refptr<ExtensionFunction> function,
                                    const std::string& args) {
    return RunFunctionInternal(second_extension_, function, args);
  }

  api_test_utils::FunctionMode GetRunMode() {
    return current_browser()->profile()->IsOffTheRecord()
               ? api_test_utils::FunctionMode::kIncognito
               : api_test_utils::FunctionMode::kNone;
  }

  // api_test_utils::RunFunction*() only uses browser for its
  // profile(), so pass it the on-record browser so that it always uses the
  // on-record profile to match real-life behavior.

  std::optional<base::Value> RunFunctionAndReturnResult(
      scoped_refptr<ExtensionFunction> function,
      const std::string& args) {
    SetUpExtensionFunction(extension_, function.get());
    return api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), args, current_browser()->profile(), GetRunMode());
  }

  std::string RunFunctionAndReturnError(
      scoped_refptr<ExtensionFunction> function,
      const std::string& args) {
    SetUpExtensionFunction(extension_, function.get());
    return api_test_utils::RunFunctionAndReturnError(
        function.get(), args, current_browser()->profile(), GetRunMode());
  }

  std::string RunFunctionAndReturnErrorInSecondExtension(
      scoped_refptr<ExtensionFunction> function,
      const std::string& args) {
    SetUpExtensionFunction(second_extension_, function.get());
    return api_test_utils::RunFunctionAndReturnError(
        function.get(), args, current_browser()->profile(), GetRunMode());
  }

  bool RunFunctionAndReturnString(scoped_refptr<ExtensionFunction> function,
                                  const std::string& args,
                                  std::string* result_string) {
    SetUpExtensionFunction(extension_, function.get());
    std::optional<base::Value> result =
        RunFunctionAndReturnResult(function, args);
    EXPECT_TRUE(result);
    if (result && result->is_string()) {
      *result_string = result->GetString();
      return true;
    }
    return false;
  }

  std::string DownloadItemIdAsArgList(const DownloadItem* download_item) {
    return base::StringPrintf("[%d]", download_item->GetId());
  }

  base::FilePath downloads_directory() {
    return DownloadPrefs(current_browser()->profile()).DownloadPath();
  }

  DownloadsEventsListener* events_listener() { return events_listener_.get(); }

  const Extension* extension() { return extension_; }

 private:
  void SetUpExtensionFunction(const Extension* extension,
                              scoped_refptr<ExtensionFunction> function) {
    if (extension) {
      const GURL url = current_browser_ == incognito_browser_ &&
                               !IncognitoInfo::IsSplitMode(extension)
                           ? GURL(url::kAboutBlankURL)
                           : extension->GetResourceURL("empty.html");
      // Watch and wait for the navigation to take place.
      auto observer = std::make_unique<content::TestNavigationObserver>(url);
      observer->WatchExistingWebContents();
      observer->StartWatchingNewWebContents();
      // Recreate the tab each time for insulation.
      content::WebContents* tab = chrome::AddSelectedTabWithURL(
          current_browser(), url, ui::PAGE_TRANSITION_LINK);
      observer->WaitForNavigationFinished();
      function->set_extension(extension);
      function->SetRenderFrameHost(tab->GetPrimaryMainFrame());
      function->set_source_process_id(
          tab->GetPrimaryMainFrame()->GetProcess()->GetID());
    }
  }

  bool RunFunctionInternal(const Extension* extension,
                           scoped_refptr<ExtensionFunction> function,
                           const std::string& args) {
    scoped_refptr<ExtensionFunction> delete_function(function);
    SetUpExtensionFunction(extension, function);
    bool result = api_test_utils::RunFunction(
        function.get(), args, current_browser()->profile(), GetRunMode());
    if (!result) {
      LOG(ERROR) << function->GetError();
    }
    return result;
  }

  const Extension* LoadExtensionInternal(const char* name,
                                         bool enable_file_access) {
    // Store the created Extension object so that we can attach it to
    // ExtensionFunctions.  Also load the extension in incognito profiles for
    // testing incognito.
    const Extension* extension = ExtensionBrowserTest::LoadExtension(
        test_data_dir_.AppendASCII(name),
        {.allow_in_incognito = true, .allow_file_access = enable_file_access});
    CHECK(extension);
    content::WebContents* tab = chrome::AddSelectedTabWithURL(
        current_browser(), extension->GetResourceURL("empty.html"),
        ui::PAGE_TRANSITION_LINK);
    EXPECT_TRUE(content::WaitForLoadStop(tab));
    EventRouter::Get(current_browser()->profile())
        ->AddEventListener(downloads::OnCreated::kEventName,
                           tab->GetPrimaryMainFrame()->GetProcess(),
                           extension->id());
    EventRouter::Get(current_browser()->profile())
        ->AddEventListener(downloads::OnChanged::kEventName,
                           tab->GetPrimaryMainFrame()->GetProcess(),
                           extension->id());
    EventRouter::Get(current_browser()->profile())
        ->AddEventListener(downloads::OnErased::kEventName,
                           tab->GetPrimaryMainFrame()->GetProcess(),
                           extension->id());
    return extension;
  }

  raw_ptr<const Extension, DanglingUntriaged> extension_;
  raw_ptr<const Extension, DanglingUntriaged> second_extension_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> current_browser_;
  std::unique_ptr<DownloadsEventsListener> events_listener_;

  std::unique_ptr<net::test_server::ControllableHttpResponse> first_download_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> second_download_;
};

namespace {

class MockIconExtractorImpl : public DownloadFileIconExtractor {
 public:
  MockIconExtractorImpl(const base::FilePath& path,
                        IconLoader::IconSize icon_size,
                        const std::string& response)
      : expected_path_(path),
        expected_icon_size_(icon_size),
        response_(response) {
  }
  ~MockIconExtractorImpl() override {}

  bool ExtractIconURLForPath(const base::FilePath& path,
                             float scale,
                             IconLoader::IconSize icon_size,
                             IconURLCallback callback) override {
    EXPECT_STREQ(expected_path_.value().c_str(), path.value().c_str());
    EXPECT_EQ(expected_icon_size_, icon_size);
    if (expected_path_ == path &&
        expected_icon_size_ == icon_size) {
      callback_ = std::move(callback);
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&MockIconExtractorImpl::RunCallback,
                                    base::Unretained(this)));
      return true;
    } else {
      return false;
    }
  }

 private:
  void RunCallback() {
    DCHECK(callback_);
    std::move(callback_).Run(response_);
    // Drop the reference on extension function to avoid memory leaks.
    callback_ = IconURLCallback();
  }

  base::FilePath             expected_path_;
  IconLoader::IconSize expected_icon_size_;
  std::string          response_;
  IconURLCallback      callback_;
};

bool ItemNotInProgress(DownloadItem* item) {
  return item->GetState() != DownloadItem::IN_PROGRESS;
}

// Cancels the underlying DownloadItem when the ScopedCancellingItem goes out of
// scope. Like a scoped_ptr, but for DownloadItems.
class ScopedCancellingItem {
 public:
  explicit ScopedCancellingItem(DownloadItem* item) : item_(item) {}

  ScopedCancellingItem(const ScopedCancellingItem&) = delete;
  ScopedCancellingItem& operator=(const ScopedCancellingItem&) = delete;

  ~ScopedCancellingItem() {
    item_->Cancel(true);
    content::DownloadUpdatedObserver observer(
        item_, base::BindRepeating(&ItemNotInProgress));
    observer.WaitForEvent();
  }
  DownloadItem* get() { return item_; }
 private:
  raw_ptr<DownloadItem> item_;
};

// Cancels all the underlying DownloadItems when the ScopedItemVectorCanceller
// goes out of scope. Generalization of ScopedCancellingItem to many
// DownloadItems.
class ScopedItemVectorCanceller {
 public:
  explicit ScopedItemVectorCanceller(DownloadManager::DownloadVector* items)
    : items_(items) {
  }

  ScopedItemVectorCanceller(const ScopedItemVectorCanceller&) = delete;
  ScopedItemVectorCanceller& operator=(const ScopedItemVectorCanceller&) =
      delete;

  ~ScopedItemVectorCanceller() {
    for (DownloadManager::DownloadVector::const_iterator item = items_->begin();
         item != items_->end(); ++item) {
      if ((*item)->GetState() == DownloadItem::IN_PROGRESS)
        (*item)->Cancel(true);
      content::DownloadUpdatedObserver observer(
          (*item), base::BindRepeating(&ItemNotInProgress));
      observer.WaitForEvent();
    }
  }

 private:
  raw_ptr<DownloadManager::DownloadVector> items_;
};

// Writes an HTML5 file so that it can be downloaded.
class HTML5FileWriter {
 public:
  static bool CreateFileForTesting(storage::FileSystemContext* context,
                                   const storage::FileSystemURL& path,
                                   const char* data,
                                   int length) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Create a temp file.
    base::FilePath temp_file;
    if (!base::CreateTemporaryFile(&temp_file) ||
        !base::WriteFile(temp_file, std::string_view(data, length))) {
      return false;
    }
    // Invoke the fileapi to copy it into the sandboxed filesystem.
    bool result = false;
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CreateFileForTestingOnIOThread,
                       base::Unretained(context), path, temp_file,
                       base::Unretained(&result), run_loop.QuitClosure()));
    // Wait for that to finish.
    run_loop.Run();
    base::DeleteFile(temp_file);
    return result;
  }

 private:
  static void CopyInCompletion(bool* result,
                               base::OnceClosure quit_closure,
                               base::File::Error error) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    *result = error == base::File::FILE_OK;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(quit_closure));
  }

  static void CreateFileForTestingOnIOThread(
      storage::FileSystemContext* context,
      const storage::FileSystemURL& path,
      const base::FilePath& temp_file,
      bool* result,
      base::OnceClosure quit_closure) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    context->operation_runner()->CopyInForeignFile(
        temp_file, path,
        base::BindOnce(&CopyInCompletion, base::Unretained(result),
                       std::move(quit_closure)));
  }
};

// TODO(benjhayden) Merge this with the other TestObservers.
class JustInProgressDownloadObserver
    : public content::DownloadTestObserverInProgress {
 public:
  JustInProgressDownloadObserver(
      DownloadManager* download_manager, size_t wait_count)
      : content::DownloadTestObserverInProgress(download_manager, wait_count) {
  }

  JustInProgressDownloadObserver(const JustInProgressDownloadObserver&) =
      delete;
  JustInProgressDownloadObserver& operator=(
      const JustInProgressDownloadObserver&) = delete;

  ~JustInProgressDownloadObserver() override {}

 private:
  bool IsDownloadInFinalState(DownloadItem* item) override {
    return item->GetState() == DownloadItem::IN_PROGRESS;
  }
};

bool ItemIsInterrupted(DownloadItem* item) {
  return item->GetState() == DownloadItem::INTERRUPTED;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadExtensionTest_Open) {
  platform_util::internal::DisableShellOperationsForTesting();

  LoadExtension("downloads_split");
  scoped_refptr<DownloadsOpenFunction> open_function =
      base::MakeRefCounted<DownloadsOpenFunction>();
  open_function->set_user_gesture(true);
  EXPECT_STREQ(errors::kInvalidId,
               RunFunctionAndReturnError(
                   open_function,
                   "[-42]").c_str());

  DownloadItem* download_item = CreateFirstSlowTestDownload();
  ASSERT_TRUE(download_item);
  EXPECT_FALSE(download_item->GetOpened());
  EXPECT_FALSE(download_item->GetOpenWhenComplete());
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"application/octet-stream\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_item->GetURL().spec().c_str())));
  open_function = base::MakeRefCounted<DownloadsOpenFunction>();
  open_function->set_user_gesture(true);
  EXPECT_STREQ(errors::kNotComplete,
               RunFunctionAndReturnError(
                   open_function,
                   DownloadItemIdAsArgList(download_item)).c_str());

  FinishFirstSlowDownloads();
  EXPECT_FALSE(download_item->GetOpened());

  open_function = base::MakeRefCounted<DownloadsOpenFunction>();
  EXPECT_STREQ(errors::kUserGesture,
               RunFunctionAndReturnError(
                  open_function,
                  DownloadItemIdAsArgList(download_item)).c_str());
  current_browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(download_item->GetOpened());

  open_function = base::MakeRefCounted<DownloadsOpenFunction>();
  open_function->set_user_gesture(true);
  base::Value::List args_list;
  args_list.Append(static_cast<int>(download_item->GetId()));
  open_function->SetArgs(std::move(args_list));
  open_function->set_extension(extension());
  DownloadsOpenFunction::OnPromptCreatedCallback callback =
      base::BindOnce(&OnOpenPromptCreated, base::Unretained(download_item));
  DownloadsOpenFunction::set_on_prompt_created_cb_for_testing(&callback);
  api_test_utils::SendResponseHelper response_helper(open_function.get());
  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(current_browser()->profile()));
  open_function->SetDispatcher(dispatcher->AsWeakPtr());
  open_function->RunWithValidation().Execute();
  response_helper.WaitForResponse();
  EXPECT_TRUE(response_helper.has_response());
  EXPECT_TRUE(response_helper.GetResponse());
  DownloadOpenObserver observer(download_item);
  observer.WaitForEvent();
  EXPECT_TRUE(download_item->GetOpened());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_PauseResumeCancelErase) {
  DownloadItem* download_item = CreateFirstSlowTestDownload();
  ASSERT_TRUE(download_item);
  std::string error;

  // Call pause().  It should succeed and the download should be paused on
  // return.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsPaused());

  // Calling removeFile on a non-active download yields kNotComplete
  // and should not crash. http://crbug.com/319984
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsRemoveFileFunction>(),
      DownloadItemIdAsArgList(download_item));
  EXPECT_STREQ(errors::kNotComplete, error.c_str());

  // Calling pause() twice shouldn't be an error.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsPaused());

  // Now try resuming this download.  It should succeed.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_FALSE(download_item->IsPaused());

  // Resume again.  Resuming a download that wasn't paused is not an error.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_FALSE(download_item->IsPaused());

  // Pause again.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsPaused());

  // And now cancel.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsCancelFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_EQ(DownloadItem::CANCELLED, download_item->GetState());

  // Cancel again.  Shouldn't have any effect.
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsCancelFunction>(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_EQ(DownloadItem::CANCELLED, download_item->GetState());

  // Calling paused on a non-active download yields kNotInProgress.
  error =
      RunFunctionAndReturnError(base::MakeRefCounted<DownloadsPauseFunction>(),
                                DownloadItemIdAsArgList(download_item));
  EXPECT_STREQ(errors::kNotInProgress, error.c_str());

  // Calling resume on a non-active download yields kNotResumable
  error =
      RunFunctionAndReturnError(base::MakeRefCounted<DownloadsResumeFunction>(),
                                DownloadItemIdAsArgList(download_item));
  EXPECT_STREQ(errors::kNotResumable, error.c_str());

  // Calling pause on a non-existent download yields kInvalidId.
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsPauseFunction>(), "[-42]");
  EXPECT_STREQ(errors::kInvalidId, error.c_str());

  // Calling resume on a non-existent download yields kInvalidId
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsResumeFunction>(), "[-42]");
  EXPECT_STREQ(errors::kInvalidId, error.c_str());

  // Calling removeFile on a non-existent download yields kInvalidId.
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsRemoveFileFunction>(), "[-42]");
  EXPECT_STREQ(errors::kInvalidId, error.c_str());

  int id = download_item->GetId();
  std::optional<base::Value> result =
      RunFunctionAndReturnResult(base::MakeRefCounted<DownloadsEraseFunction>(),
                                 base::StringPrintf("[{\"id\": %d}]", id));
  DownloadManager::DownloadVector items;
  GetCurrentManager()->GetAllDownloads(&items);
  EXPECT_EQ(0UL, items.size());
  ASSERT_TRUE(result);
  download_item = nullptr;
  ASSERT_TRUE(result->is_list());
  const base::Value::List& result_list = result->GetList();
  ASSERT_EQ(1UL, result_list.size());
  ASSERT_TRUE(result_list[0].is_int());
  EXPECT_EQ(id, result_list[0].GetInt());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Open_Remove_Open) {
  static const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("file.txt"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS}};
  DownloadManager::DownloadVector all_downloads;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo),
                                     &all_downloads));
  DownloadItem* download_item = all_downloads[0];
  ASSERT_TRUE(download_item);
  EXPECT_FALSE(download_item->GetFileExternallyRemoved());
  EXPECT_FALSE(download_item->GetOpened());
  EXPECT_FALSE(download_item->GetOpenWhenComplete());

  RunFunction(base::MakeRefCounted<DownloadsRemoveFileFunction>(),
              DownloadItemIdAsArgList(download_item));
  EXPECT_TRUE(download_item->GetFileExternallyRemoved());
  EXPECT_FALSE(download_item->GetOpened());
  EXPECT_FALSE(download_item->GetOpenWhenComplete());

  scoped_refptr<DownloadsOpenFunction> open_function =
      base::MakeRefCounted<DownloadsOpenFunction>();
  open_function->set_user_gesture(true);
  EXPECT_STREQ(errors::kFileAlreadyDeleted,
               RunFunctionAndReturnError(open_function,
                                         DownloadItemIdAsArgList(download_item))
                   .c_str());
  EXPECT_TRUE(download_item->GetFileExternallyRemoved());
  EXPECT_FALSE(download_item->GetOpened());
  EXPECT_FALSE(download_item->GetOpenWhenComplete());
}

scoped_refptr<ExtensionFunction> MockedGetFileIconFunction(
    const base::FilePath& expected_path,
    IconLoader::IconSize icon_size,
    const std::string& response) {
  scoped_refptr<DownloadsGetFileIconFunction> function(
      base::MakeRefCounted<DownloadsGetFileIconFunction>());
  function->SetIconExtractorForTesting(new MockIconExtractorImpl(
      expected_path, icon_size, response));
  return function;
}

// Test downloads.getFileIcon() on in-progress, finished, cancelled and deleted
// download items.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_FileIcon_Active) {
  DownloadItem* download_item = CreateFirstSlowTestDownload();
  ASSERT_TRUE(download_item);
  ASSERT_FALSE(download_item->GetTargetFilePath().empty());
  std::string args32(base::StringPrintf("[%d, {\"size\": 32}]",
                     download_item->GetId()));
  std::string result_string;

  // Get the icon for the in-progress download.  This call should succeed even
  // if the file type isn't registered.
  // Test whether the correct path is being pased into the icon extractor.
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          download_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      base::StringPrintf("[%d, {}]", download_item->GetId()), &result_string));

  // Now try a 16x16 icon.
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          download_item->GetTargetFilePath(), IconLoader::SMALL, "foo"),
      base::StringPrintf("[%d, {\"size\": 16}]", download_item->GetId()),
      &result_string));

  // Explicitly asking for 32x32 should give us a 32x32 icon.
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          download_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      args32, &result_string));

  // Asking for an icon that is neither 16 nor 32 px should fail.
  // Regression test for https://crbug.com/348379083.
  EXPECT_EQ(
      RunFunctionAndReturnError(
          MockedGetFileIconFunction(download_item->GetTargetFilePath(),
                                    IconLoader::NORMAL, "foo"),
          base::StringPrintf(R"([%d, {"size": 10}])", download_item->GetId())),
      "Invalid `size`. Must be either `16` or `32`.");

  // Finish the download and try again.
  FinishFirstSlowDownloads();
  EXPECT_EQ(DownloadItem::COMPLETE, download_item->GetState());
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          download_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      args32, &result_string));

  // Check the path passed to the icon extractor post-completion.
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          download_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      args32, &result_string));
  download_item->Remove();

  // Now create another download.
  download_item = CreateSecondSlowTestDownload();
  ASSERT_TRUE(download_item);
  ASSERT_FALSE(download_item->GetTargetFilePath().empty());
  args32 = base::StringPrintf("[%d, {\"size\": 32}]", download_item->GetId());

  // Cancel the download. As long as the download has a target path, we should
  // be able to query the file icon.
  download_item->Cancel(true);
  ASSERT_FALSE(download_item->GetTargetFilePath().empty());
  // Let cleanup complete on blocking threads.
  content::RunAllTasksUntilIdle();
  // Check the path passed to the icon extractor post-cancellation.
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          download_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      args32,
      &result_string));

  // Simulate an error during icon load by invoking the mock with an empty
  // result string.
  std::string error = RunFunctionAndReturnError(
      MockedGetFileIconFunction(download_item->GetTargetFilePath(),
                                IconLoader::NORMAL,
                                std::string()),
      args32);
  EXPECT_STREQ(errors::kIconNotFound, error.c_str());

  // Once the download item is deleted, we should return kInvalidId.
  int id = download_item->GetId();
  download_item->Remove();
  download_item = nullptr;
  EXPECT_EQ(nullptr, GetCurrentManager()->GetDownload(id));
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsGetFileIconFunction>(), args32);
  EXPECT_STREQ(errors::kInvalidId,
               error.c_str());
}

// Test that we can acquire file icons for history downloads regardless of
// whether they exist or not.  If the file doesn't exist we should receive a
// generic icon from the OS/toolkit that may or may not be specific to the file
// type.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_FileIcon_History) {
  const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("real.txt"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},
      {FILE_PATH_LITERAL("fake.txt"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS}};
  DownloadManager::DownloadVector all_downloads;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo),
                                     &all_downloads));

  base::FilePath real_path = all_downloads[0]->GetTargetFilePath();
  base::FilePath fake_path = all_downloads[1]->GetTargetFilePath();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(real_path, ""));
    ASSERT_TRUE(base::PathExists(real_path));
    ASSERT_FALSE(base::PathExists(fake_path));
  }

  for (const download::DownloadItem* download : all_downloads) {
    std::string result_string;
    // Use a MockIconExtractorImpl to test if the correct path is being passed
    // into the DownloadFileIconExtractor.
    EXPECT_TRUE(RunFunctionAndReturnString(
        MockedGetFileIconFunction(download->GetTargetFilePath(),
                                  IconLoader::NORMAL, "hello"),
        base::StringPrintf("[%d, {\"size\": 32}]", download->GetId()),
        &result_string));
    EXPECT_STREQ("hello", result_string.c_str());
  }
}

// Test passing the empty query to search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_SearchEmptyQuery) {
  ScopedCancellingItem item(CreateFirstSlowTestDownload());
  ASSERT_TRUE(item.get());

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
}

// Test that file existence check should be performed after search.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, FileExistenceCheckAfterSearch) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  DownloadItem* download_item = CreateFirstSlowTestDownload();
  ASSERT_TRUE(download_item);
  ASSERT_FALSE(download_item->GetTargetFilePath().empty());

  // Finish the download and try again.
  FinishFirstSlowDownloads();
  base::DeleteFile(download_item->GetTargetFilePath());

  ASSERT_FALSE(download_item->GetFileExternallyRemoved());
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());

  // Check file removal update will eventually come. WaitForEvent() will
  // immediately return if the file is already removed.
  content::DownloadUpdatedObserver(
      download_item, base::BindRepeating(&IsDownloadExternallyRemoved))
      .WaitForEvent();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadsShowFunction) {
  platform_util::internal::DisableShellOperationsForTesting();
  ScopedCancellingItem item(CreateFirstSlowTestDownload());
  ASSERT_TRUE(item.get());

  RunFunction(base::MakeRefCounted<DownloadsShowFunction>(),
              DownloadItemIdAsArgList(item.get()));
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadsShowDefaultFolderFunction) {
  platform_util::internal::DisableShellOperationsForTesting();
  ScopedCancellingItem item(CreateFirstSlowTestDownload());
  ASSERT_TRUE(item.get());

  RunFunction(base::MakeRefCounted<DownloadsShowDefaultFolderFunction>(),
              DownloadItemIdAsArgList(item.get()));
}
#endif


// Test the |filenameRegex| parameter for search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_SearchFilenameRegex) {
  const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("foobar"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},
      {FILE_PATH_LITERAL("baz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS}};
  DownloadManager::DownloadVector all_downloads;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo),
                                     &all_downloads));

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      "[{\"filenameRegex\": \"foobar\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
  const base::Value& item_value = result->GetList()[0];
  ASSERT_TRUE(item_value.is_dict());
  std::optional<int> item_id = item_value.GetDict().FindInt("id");
  ASSERT_TRUE(item_id);
  ASSERT_EQ(all_downloads[0]->GetId(), static_cast<uint32_t>(*item_id));
}

// Test the |id| parameter for search().
//
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadExtensionTest_SearchId) {
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      base::StringPrintf("[{\"id\": %u}]", items[0]->GetId()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
  const base::Value& item_value = result->GetList()[0];
  ASSERT_TRUE(item_value.is_dict());
  std::optional<int> item_id = item_value.GetDict().FindInt("id");
  ASSERT_TRUE(item_id);
  ASSERT_EQ(items[0]->GetId(), static_cast<uint32_t>(*item_id));
}

// Test specifying both the |id| and |filename| parameters for search().
//
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_SearchIdAndFilename) {
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      "[{\"id\": 0, \"filename\": \"foobar\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(0UL, result->GetList().size());
}

// Test a single |orderBy| parameter for search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_SearchOrderBy) {
  const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("zzz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},
      {FILE_PATH_LITERAL("baz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS}};
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(
      CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo), &items));

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      "[{\"orderBy\": [\"filename\"]}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(2UL, result->GetList().size());
  const base::Value& item0_value = result->GetList()[0];
  const base::Value& item1_value = result->GetList()[1];
  ASSERT_TRUE(item0_value.is_dict());
  ASSERT_TRUE(item1_value.is_dict());
  const std::string* item0_name = item0_value.GetDict().FindString("filename");
  const std::string* item1_name = item1_value.GetDict().FindString("filename");
  ASSERT_TRUE(item0_name);
  ASSERT_TRUE(item1_name);
  ASSERT_GT(items[0]->GetTargetFilePath().value(),
            items[1]->GetTargetFilePath().value());
  ASSERT_LT(*item0_name, *item1_name);
}

// Test specifying an empty |orderBy| parameter for search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_SearchOrderByEmpty) {
  const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("zzz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},
      {FILE_PATH_LITERAL("baz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS}};
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(
      CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo), &items));

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{\"orderBy\": []}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(2UL, result->GetList().size());
  const base::Value& item0_value = result->GetList()[0];
  const base::Value& item1_value = result->GetList()[1];
  ASSERT_TRUE(item0_value.is_dict());
  ASSERT_TRUE(item1_value.is_dict());
  const std::string* item0_name = item0_value.GetDict().FindString("filename");
  const std::string* item1_name = item1_value.GetDict().FindString("filename");
  ASSERT_TRUE(item0_name);
  ASSERT_TRUE(item1_name);
  ASSERT_GT(items[0]->GetTargetFilePath().value(),
            items[1]->GetTargetFilePath().value());
  // The order of results when orderBy is empty is unspecified. When there are
  // no sorters, DownloadQuery does not call sort(), so the order of the results
  // depends on the order of the items in DownloadManagerImpl::downloads_,
  // which is unspecified and differs between libc++ and libstdc++.
  // http://crbug.com/365334
}

// Test the |danger| option for search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_SearchDanger) {
  const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("zzz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT},
      {FILE_PATH_LITERAL("baz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS}};
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(
      CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo), &items));

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      "[{\"danger\": \"content\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
}

// Test the |state| option for search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_SearchState) {
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);

  items[0]->Cancel(true);

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      "[{\"state\": \"in_progress\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
}

// Test the |limit| option for search().
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_SearchLimit) {
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{\"limit\": 1}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
}

// Test invalid search parameters.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_SearchInvalid) {
  std::string error =
      RunFunctionAndReturnError(base::MakeRefCounted<DownloadsSearchFunction>(),
                                "[{\"filenameRegex\": \"(\"}]");
  EXPECT_STREQ(errors::kInvalidFilter,
      error.c_str());
  error =
      RunFunctionAndReturnError(base::MakeRefCounted<DownloadsSearchFunction>(),
                                "[{\"orderBy\": [\"goat\"]}]");
  EXPECT_STREQ(errors::kInvalidOrderBy,
      error.c_str());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{\"limit\": -1}]");
  EXPECT_STREQ(errors::kInvalidQueryLimit,
      error.c_str());
}

// Test searching using multiple conditions through multiple downloads.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadExtensionTest_SearchPlural) {
  const HistoryDownloadInfo kHistoryInfo[] = {
      {FILE_PATH_LITERAL("aaa"), DownloadItem::CANCELLED,
       download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS},
      {FILE_PATH_LITERAL("zzz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT},
      {FILE_PATH_LITERAL("baz"), DownloadItem::COMPLETE,
       download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT},
  };
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(
      CreateHistoryDownloads(kHistoryInfo, std::size(kHistoryInfo), &items));

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(),
      "[{"
      "\"state\": \"complete\", "
      "\"danger\": \"content\", "
      "\"orderBy\": [\"filename\"], "
      "\"limit\": 1}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  ASSERT_EQ(1UL, result->GetList().size());
  const base::Value& item_value = result->GetList()[0];
  ASSERT_TRUE(item_value.is_dict());
  const std::string* item_name = item_value.GetDict().FindString("filename");
  ASSERT_TRUE(item_name);
  ASSERT_EQ(items[2]->GetTargetFilePath().AsUTF8Unsafe(), *item_name);
}

// Test that incognito downloads are only visible in incognito contexts, and
// test that on-record downloads are visible in both incognito and on-record
// contexts, for DownloadsSearchFunction, DownloadsPauseFunction,
// DownloadsResumeFunction, and DownloadsCancelFunction.
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_SearchPauseResumeCancelGetFileIconIncognito) {
  std::optional<base::Value> result_value;
  std::string error;
  std::string result_string;

  // Set up one on-record item and one off-record item.
  // Set up the off-record item first because otherwise there are mysteriously 3
  // items total instead of 2.
  // TODO(benjhayden): Figure out where the third item comes from.
  GoOffTheRecord();
  DownloadItem* off_item = CreateFirstSlowTestDownload();
  ASSERT_TRUE(off_item);
  const std::string off_item_arg = DownloadItemIdAsArgList(off_item);

  GoOnTheRecord();
  DownloadItem* on_item = CreateSecondSlowTestDownload();
  ASSERT_TRUE(on_item);
  const std::string on_item_arg = DownloadItemIdAsArgList(on_item);
  ASSERT_TRUE(on_item->GetTargetFilePath() != off_item->GetTargetFilePath());

  // Extensions running in the incognito window should have access to both
  // items because the Test extension is in spanning mode.
  GoOffTheRecord();
  result_value = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{}]");
  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_list());
  ASSERT_EQ(2UL, result_value->GetList().size());
  {
    const base::Value::Dict& result_dict = result_value->GetList()[0].GetDict();
    const std::string* filename = result_dict.FindString("filename");
    ASSERT_TRUE(filename);
    std::optional<bool> is_incognito = result_dict.FindBool("incognito");
    ASSERT_TRUE(is_incognito.has_value());
    EXPECT_TRUE(on_item->GetTargetFilePath() ==
                base::FilePath::FromUTF8Unsafe(*filename));
    EXPECT_FALSE(is_incognito.value());
  }
  {
    const base::Value::Dict& result_dict = result_value->GetList()[1].GetDict();
    const std::string* filename = result_dict.FindString("filename");
    ASSERT_TRUE(filename);
    std::optional<bool> is_incognito = result_dict.FindBool("incognito");
    ASSERT_TRUE(is_incognito.has_value());
    EXPECT_TRUE(off_item->GetTargetFilePath() ==
                base::FilePath::FromUTF8Unsafe(*filename));
    EXPECT_TRUE(is_incognito.value());
  }

  // Extensions running in the on-record window should have access only to the
  // on-record item.
  GoOnTheRecord();
  result_value = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsSearchFunction>(), "[{}]");
  ASSERT_TRUE(result_value);
  ASSERT_TRUE(result_value->is_list());
  ASSERT_EQ(1UL, result_value->GetList().size());
  {
    const base::Value::Dict& result_dict = result_value->GetList()[0].GetDict();
    const std::string* filename = result_dict.FindString("filename");
    ASSERT_TRUE(filename);
    EXPECT_TRUE(on_item->GetTargetFilePath() ==
                base::FilePath::FromUTF8Unsafe(*filename));
    std::optional<bool> is_incognito = result_dict.FindBool("incognito");
    ASSERT_TRUE(is_incognito.has_value());
    EXPECT_FALSE(is_incognito.value());
  }

  // Pausing/Resuming the off-record item while on the record should return an
  // error. Cancelling "non-existent" downloads is not an error.
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsPauseFunction>(), off_item_arg);
  EXPECT_STREQ(errors::kInvalidId,
               error.c_str());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsResumeFunction>(), off_item_arg);
  EXPECT_STREQ(errors::kInvalidId,
               error.c_str());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsGetFileIconFunction>(),
      base::StringPrintf("[%d, {}]", off_item->GetId()));
  EXPECT_STREQ(errors::kInvalidId,
               error.c_str());

  GoOffTheRecord();

  // Do the FileIcon test for both the on- and off-items while off the record.
  // NOTE(benjhayden): This does not include the FileIcon test from history,
  // just active downloads. This shouldn't be a problem.
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          on_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      base::StringPrintf("[%d, {}]", on_item->GetId()), &result_string));
  EXPECT_TRUE(RunFunctionAndReturnString(MockedGetFileIconFunction(
          off_item->GetTargetFilePath(), IconLoader::NORMAL, "foo"),
      base::StringPrintf("[%d, {}]", off_item->GetId()), &result_string));

  // Do the pause/resume/cancel test for both the on- and off-items while off
  // the record.
  EXPECT_TRUE(
      RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(), on_item_arg));
  EXPECT_TRUE(on_item->IsPaused());
  EXPECT_TRUE(
      RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(), on_item_arg));
  EXPECT_TRUE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          on_item_arg));
  EXPECT_FALSE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          on_item_arg));
  EXPECT_FALSE(on_item->IsPaused());
  EXPECT_TRUE(
      RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(), on_item_arg));
  EXPECT_TRUE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsCancelFunction>(),
                          on_item_arg));
  EXPECT_EQ(DownloadItem::CANCELLED, on_item->GetState());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsCancelFunction>(),
                          on_item_arg));
  EXPECT_EQ(DownloadItem::CANCELLED, on_item->GetState());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsPauseFunction>(), on_item_arg);
  EXPECT_STREQ(errors::kNotInProgress, error.c_str());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsResumeFunction>(), on_item_arg);
  EXPECT_STREQ(errors::kNotResumable, error.c_str());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(),
                          off_item_arg));
  EXPECT_TRUE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(),
                          off_item_arg));
  EXPECT_TRUE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          off_item_arg));
  EXPECT_FALSE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          off_item_arg));
  EXPECT_FALSE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsPauseFunction>(),
                          off_item_arg));
  EXPECT_TRUE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsCancelFunction>(),
                          off_item_arg));
  EXPECT_EQ(DownloadItem::CANCELLED, off_item->GetState());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsCancelFunction>(),
                          off_item_arg));
  EXPECT_EQ(DownloadItem::CANCELLED, off_item->GetState());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsPauseFunction>(), off_item_arg);
  EXPECT_STREQ(errors::kNotInProgress, error.c_str());
  error = RunFunctionAndReturnError(
      base::MakeRefCounted<DownloadsResumeFunction>(), off_item_arg);
  EXPECT_STREQ(errors::kNotResumable, error.c_str());
}

// Test that we can start a download and that the correct sequence of events is
// fired for it.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Basic) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();
  GoOnTheRecord();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());
  ASSERT_EQ(GetExtensionStoragePartitionConfig(),
            GetCurrentManager()
                ->SerializedEmbedderDownloadDataToStoragePartitionConfig(
                    item->GetSerializedEmbedderDownloadData()));

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"finalUrl\": \"%s\","
                          "  \"url\": \"%s\"}]",
                          download_url.c_str(),
                          download_url.c_str())));
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that we can start a download that gets redirected and that the correct
// sequence of events is fired for it.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Redirect) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL download_final_url(embedded_test_server()->GetURL("/slow?0"));
  GURL download_url(embedded_test_server()->GetURL("/server-redirect?" +
                                                   download_final_url.spec()));

  GoOnTheRecord();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.spec().c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl());
  ASSERT_EQ(GetExtensionStoragePartitionConfig(),
            GetCurrentManager()
                ->SerializedEmbedderDownloadDataToStoragePartitionConfig(
                    item->GetSerializedEmbedderDownloadData()));

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"finalUrl\": \"%s\","
                          "  \"url\": \"%s\"}]",
                          download_final_url.spec().c_str(),
                          download_url.spec().c_str())));
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that we can start a download from an incognito context, and that the
// download knows that it's incognito.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Incognito) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOffTheRecord();
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(GetCurrentManager()->GetBrowserContext()->IsOffTheRecord());
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": true,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\":%d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\":%d,"
                          "  \"state\": {"
                          "    \"current\": \"complete\","
                          "    \"previous\": \"in_progress\"}}]",
                          result_id)));
}

// Test that if file name with disallowed characters are provided, the
// characters will be replaced.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Disallowed_Character_In_Filename) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();
  GoOnTheRecord();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\", \"filename\": \"foo%%bar\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());
  std::unique_ptr<content::DownloadTestObserver> obs(CreateDownloadObserver(1));
  obs->WaitForFinished();
  EXPECT_EQ(FILE_PATH_LITERAL("foo_bar"),
            item->GetFileNameToReportUser().value());
}

namespace {

class CustomResponse : public net::test_server::HttpResponse {
 public:
  CustomResponse(base::OnceClosure* callback,
                 base::TaskRunner** task_runner,
                 bool first_request)
      : callback_(callback),
        task_runner_(task_runner),
        first_request_(first_request) {}

  CustomResponse(const CustomResponse&) = delete;
  CustomResponse& operator=(const CustomResponse&) = delete;

  ~CustomResponse() override {}

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    base::StringPairs headers = {
        // "HTTP/1.1 200 OK\r\n"
        {"Content-type", "application/octet-stream"},
        {"Cache-Control", "max-age=0"},
    };
    std::string contents = std::string(kDownloadSize, '*');

    if (first_request_) {
      *callback_ = base::BindOnce(
          &net::test_server::HttpResponseDelegate::FinishResponse, delegate);
      *task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault().get();
      delegate->SendResponseHeaders(net::HTTP_OK, "OK", headers);
      delegate->SendContents(contents);
    } else {
      delegate->SendHeadersContentAndFinish(net::HTTP_OK, "OK", headers,
                                            contents);
    }
  }

 private:
  raw_ptr<base::OnceClosure> callback_;
  raw_ptr<base::TaskRunner*> task_runner_;
  bool first_request_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_InterruptAndResume) {
  LoadExtension("downloads_split");

  DownloadItem* item = nullptr;

  base::OnceClosure complete_callback;
  base::TaskRunner* embedded_test_server_io_runner = nullptr;
  const char kThirdDownloadUrl[] = "/download3";
  bool first_request = true;
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        std::unique_ptr<net::test_server::HttpResponse> rv;
        if (request.relative_url == kThirdDownloadUrl) {
          rv = std::make_unique<CustomResponse>(&complete_callback,
                                                &embedded_test_server_io_runner,
                                                first_request);
          first_request = false;
        }
        return rv;
      }));

  StartEmbeddedTestServer();
  const GURL download_url = embedded_test_server()->GetURL(kThirdDownloadUrl);

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.spec().c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl());
  ASSERT_EQ(GetExtensionStoragePartitionConfig(),
            GetCurrentManager()
                ->SerializedEmbedderDownloadDataToStoragePartitionConfig(
                    item->GetSerializedEmbedderDownloadData()));

  item->SimulateErrorForTesting(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  embedded_test_server_io_runner->PostTask(FROM_HERE,
                                           std::move(complete_callback));

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf("[{\"id\":%d,"
                                         "  \"state\": {"
                                         "    \"current\": \"interrupted\","
                                         "    \"previous\": \"in_progress\"}}]",
                                         result_id)));

  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsResumeFunction>(),
                          DownloadItemIdAsArgList(item)));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf("[{\"id\":%d,"
                                         "  \"state\": {"
                                         "    \"current\": \"in_progress\","
                                         "    \"previous\": \"interrupted\"}}]",
                                         result_id)));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf("[{\"id\":%d,"
                                         "  \"state\": {"
                                         "    \"current\": \"complete\","
                                         "    \"previous\": \"in_progress\"}}]",
                                         result_id)));
}

// Test that we disallow certain headers case-insensitively.
// TODO(crbug.com/335421977): Flaky on "Linux ChromiumOS MSan Tests"
#if (BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER))
#define MAYBE_DownloadExtensionTest_Download_UnsafeHeaders \
  DISABLED_DownloadExtensionTest_Download_UnsafeHeaders
#else
#define MAYBE_DownloadExtensionTest_Download_UnsafeHeaders \
  DownloadExtensionTest_Download_UnsafeHeaders
#endif
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       MAYBE_DownloadExtensionTest_Download_UnsafeHeaders) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOnTheRecord();

  static const char* const kUnsafeHeaders[] = {
      "Accept-chArsEt",
      "accept-eNcoding",
      "coNNection",
      "coNteNt-leNgth",
      "cooKIE",
      "cOOkie2",
      "dAtE",
      "DNT",
      "ExpEcT",
      "hOsT",
      "kEEp-aLivE",
      "rEfErEr",
      "sEt-cOoKiE",
      "tE",
      "trAilER",
      "trANsfer-eNcodiNg",
      "upGRAde",
      "usER-agENt",
      "viA",
      "pRoxY-",
      "sEc-",
      "pRoxY-probably-not-evil",
      "sEc-probably-not-evil",
      "oRiGiN",
      "Access-Control-Request-Headers",
      "Access-Control-Request-Method",
      "Access-Control-Request-Private-Network",
  };

  for (size_t index = 0; index < std::size(kUnsafeHeaders); ++index) {
    std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();
    EXPECT_STREQ(
        errors::kInvalidHeaderUnsafe,
        RunFunctionAndReturnError(
            base::MakeRefCounted<DownloadsDownloadFunction>(),
            base::StringPrintf("[{\"url\": \"%s\","
                               "  \"filename\": \"unsafe-header-%d.txt\","
                               "  \"headers\": [{"
                               "    \"name\": \"%s\","
                               "    \"value\": \"unsafe\"}]}]",
                               download_url.c_str(), static_cast<int>(index),
                               kUnsafeHeaders[index]))
            .c_str());
  }
}

// Tests that invalid header names and values are rejected.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_InvalidHeaders) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOnTheRecord();
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();
  EXPECT_STREQ(
      errors::kInvalidHeaderName,
      RunFunctionAndReturnError(
          base::MakeRefCounted<DownloadsDownloadFunction>(),
          base::StringPrintf(
              "[{\"url\": \"%s\","
              "  \"filename\": \"unsafe-header-crlf.txt\","
              "  \"headers\": [{"
              "    \"name\": \"Header\\r\\nSec-Spoof: Hey\\r\\nX-Split:X\","
              "    \"value\": \"unsafe\"}]}]",
              download_url.c_str()))
          .c_str());

  EXPECT_STREQ(
      errors::kInvalidHeaderValue,
      RunFunctionAndReturnError(
          base::MakeRefCounted<DownloadsDownloadFunction>(),
          base::StringPrintf("[{\"url\": \"%s\","
                             "  \"filename\": \"unsafe-header-crlf.txt\","
                             "  \"headers\": [{"
                             "    \"name\": \"Invalid-value\","
                             "    \"value\": \"hey\\r\\nSec-Spoof: Hey\"}]}]",
                             download_url.c_str()))
          .c_str());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_DownloadExtensionTest_Download_Subdirectory \
  DISABLED_DownloadExtensionTest_Download_Subdirectory
#else
#define MAYBE_DownloadExtensionTest_Download_Subdirectory \
  DownloadExtensionTest_Download_Subdirectory
#endif
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       MAYBE_DownloadExtensionTest_Download_Subdirectory) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"sub/dir/ect/ory.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("sub/dir/ect/ory.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that invalid filenames are disallowed.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_InvalidFilename) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();
  GoOnTheRecord();

  EXPECT_STREQ(
      errors::kInvalidFilename,
      RunFunctionAndReturnError(
          base::MakeRefCounted<DownloadsDownloadFunction>(),
          base::StringPrintf("[{\"url\": \"%s\","
                             "  \"filename\": \"../../../../../etc/passwd\"}]",
                             download_url.c_str()))
          .c_str());
}

// Test that downloading invalid URLs immediately returns kInvalidURLError.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_InvalidURLs1) {
  static constexpr const char* kInvalidURLs[] = {
      "foo bar", "../hello",          "/hello",      "http://",
      "#frag",   "foo/bar.html#frag", "google.com/",
  };

  for (const char* url : kInvalidURLs) {
    EXPECT_STREQ(errors::kInvalidURL,
                 RunFunctionAndReturnError(
                     base::MakeRefCounted<DownloadsDownloadFunction>(),
                     base::StringPrintf("[{\"url\": \"%s\"}]", url))
                     .c_str())
        << url;
  }
}

// Test various failure modes for downloading invalid URLs.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_InvalidURLs2) {
  LoadExtension("downloads_split");
  GoOnTheRecord();

  int result_id = -1;
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      "[{\"url\": \"javascript:document.write(\\\"hello\\\");\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ASSERT_TRUE(WaitForInterruption(
      item, download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
      "[{\"state\": \"in_progress\","
      "  \"url\": \"javascript:document.write(\\\"hello\\\");\"}]"));

  result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      "[{\"url\": \"javascript:return false;\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ASSERT_TRUE(WaitForInterruption(
      item, download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
      "[{\"state\": \"in_progress\","
      "  \"url\": \"javascript:return false;\"}]"));
}

// Valid URLs plus fragments are still valid URLs.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_URLFragment) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url =
      embedded_test_server()->GetURL("/slow?0#fragment").spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// conflictAction may be specified without filename.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_ConflictAction) {
  static char kFilename[] = "download.txt";
  LoadExtension("downloads_split");
  std::string download_url = "data:text/plain,hello";
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename(kFilename).c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));

  result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf(
          "[{\"url\": \"%s\",  \"conflictAction\": \"overwrite\"}]",
          download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller2(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename(kFilename).c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Valid data URLs are valid URLs.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_DataURL) {
  LoadExtension("downloads_split");
  std::string download_url = "data:text/plain,hello";
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"data.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("data.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Valid file URLs are valid URLs.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_File) {
  GoOnTheRecord();
  LoadExtension("downloads_split", /*enable_file_access=*/true);
  std::string download_url = "file:///";
#if BUILDFLAG(IS_WIN)
  download_url += "C:/";
#endif

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"file.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/html\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  // Extension cannot change generated file names for file URLs. Some of the
  // platform may use .htm instead of .html.
  base::FilePath::StringType extension;
  net::GetPreferredExtensionForMimeType("text/html", &extension);
  base::FilePath expected_name =
      base::FilePath(FILE_PATH_LITERAL("download")).AddExtension(extension);

  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf(
                  "[{\"id\": %d,"
                  "  \"filename\": {"
                  "    \"previous\": \"\","
                  "    \"current\": \"%s\"}}]",
                  result_id,
                  GetFilename(expected_name.AsUTF8Unsafe().c_str()).c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that auth-basic-succeed would fail if the resource requires the
// Authorization header and chrome fails to propagate it back to the server.
// This tests both that testserver.py does not succeed when it should fail as
// well as how the downloads extension API exposes the failure to extensions.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_AuthBasic_Fail) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url =
      embedded_test_server()->GetURL("/auth-basic").spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"auth-basic-fail.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitForInterruption(
      item, download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED,
      base::StringPrintf("[{\"danger\": \"safe\","
                         "  \"incognito\": false,"
                         "  \"paused\": false,"
                         "  \"url\": \"%s\"}]",
                         download_url.c_str())));
}

// Test that DownloadsDownloadFunction propagates |headers| to the URLRequest.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Headers) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url =
      embedded_test_server()
          ->GetURL(
              "/downloads/"
              "a_zip_file.zip?expected_headers=Foo:bar&expected_headers=Qx:yo")
          .spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"headers-succeed.txt\","
                         "  \"headers\": ["
                         "    {\"name\": \"Foo\", \"value\": \"bar\"},"
                         "    {\"name\": \"Qx\", \"value\":\"yo\"}]}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"application/octet-stream\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("headers-succeed.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that headers-succeed would fail if the resource requires the headers and
// chrome fails to propagate them back to the server.  This tests both that
// testserver.py does not succeed when it should fail as well as how the
// downloads extension api exposes the failure to extensions.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Headers_Fail) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url =
      embedded_test_server()
          ->GetURL(
              "/downloads/"
              "a_zip_file.zip?expected_headers=Foo:bar&expected_headers=Qx:yo")
          .spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"headers-fail.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitForInterruption(
      item, download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
      base::StringPrintf("[{\"danger\": \"safe\","
                         "  \"incognito\": false,"
                         "  \"bytesReceived\": 0.0,"
                         "  \"fileSize\": 0.0,"
                         "  \"mime\": \"text/plain\","
                         "  \"paused\": false,"
                         "  \"url\": \"%s\"}]",
                         download_url.c_str())));
}

// Test that DownloadsDownloadFunction propagates the Authorization header
// correctly.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_AuthBasic) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url =
      embedded_test_server()->GetURL("/auth-basic").spec();
  // This is just base64 of 'username:secret'.
  static const char kAuthorization[] = "dXNlcm5hbWU6c2VjcmV0";
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"auth-basic-succeed.txt\","
                         "  \"headers\": [{"
                         "    \"name\": \"Authorization\","
                         "    \"value\": \"Basic %s\"}]}]",
                         download_url.c_str(), kAuthorization));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"bytesReceived\": 0.0,"
                          "  \"mime\": \"text/html\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that DownloadsDownloadFunction propagates the |method| and |body|
// parameters to the URLRequest.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Post) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()
                                 ->GetURL(
                                     "/post/downloads/"
                                     "a_zip_file.zip?expected_body=BODY")
                                 .spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"filename\": \"post-succeed.txt\","
                         "  \"method\": \"POST\","
                         "  \"body\": \"BODY\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"application/octet-stream\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("post-succeed.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Test that downloadPostSuccess would fail if the resource requires the POST
// method, and chrome fails to propagate the |method| parameter back to the
// server. This tests both that testserver.py does not succeed when it should
// fail, and this tests how the downloads extension api exposes the failure to
// extensions.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Post_Get) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()
                                 ->GetURL(
                                     "/post/downloads/"
                                     "a_zip_file.zip?expected_body=BODY")
                                 .spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"body\": \"BODY\","
                         "  \"filename\": \"post-get.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitForInterruption(
      item, download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
      base::StringPrintf("[{\"danger\": \"safe\","
                         "  \"incognito\": false,"
                         "  \"mime\": \"text/plain\","
                         "  \"paused\": false,"
                         "  \"id\": %d,"
                         "  \"url\": \"%s\"}]",
                         result_id, download_url.c_str())));
}

// Test that downloadPostSuccess would fail if the resource requires the POST
// method, and chrome fails to propagate the |body| parameter back to the
// server. This tests both that testserver.py does not succeed when it should
// fail, and this tests how the downloads extension api exposes the failure to
// extensions.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Post_NoBody) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()
                                 ->GetURL(
                                     "/post/downloads/"
                                     "a_zip_file.zip?expected_body=BODY")
                                 .spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\","
                         "  \"method\": \"POST\","
                         "  \"filename\": \"post-nobody.txt\"}]",
                         download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitForInterruption(
      item, download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
      base::StringPrintf("[{\"danger\": \"safe\","
                         "  \"incognito\": false,"
                         "  \"mime\": \"text/plain\","
                         "  \"paused\": false,"
                         "  \"id\": %d,"
                         "  \"url\": \"%s\"}]",
                         result_id, download_url.c_str())));
}

// Test that cancel()ing an in-progress download causes its state to transition
// to interrupted, and test that that state transition is detectable by an
// onChanged event listener.  TODO(benjhayden): Test other sources of
// interruptions such as server death.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_Download_Cancel) {
  LoadExtension("downloads_split");
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url =
      embedded_test_server()->GetURL("/download-known-size").spec();
  GoOnTheRecord();

  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"application/octet-stream\","
                          "  \"paused\": false,"
                          "  \"id\": %d,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  item->Cancel(true);
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"error\": {\"current\":\"USER_CANCELED\"},"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"interrupted\"}}]",
                          result_id)));
}

// TODO(crbug.com/41119270): Flaky on macOS
#if BUILDFLAG(IS_MAC)
#define MAYBE_DownloadExtensionTest_Download_FileSystemURL \
        DISABLED_DownloadExtensionTest_Download_FileSystemURL
#else
#define MAYBE_DownloadExtensionTest_Download_FileSystemURL \
        DownloadExtensionTest_Download_FileSystemURL
#endif

// Test downloading filesystem: URLs.
// NOTE: chrome disallows creating HTML5 FileSystem Files in incognito.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       MAYBE_DownloadExtensionTest_Download_FileSystemURL) {
  static const char kPayloadData[] = "on the record\ndata";
  GoOnTheRecord();
  LoadExtension("downloads_split");

  const std::string download_url = "filesystem:" + GetExtensionURL() +
    "temporary/on_record.txt";

  // Setup a file in the filesystem which we can download.
  ASSERT_TRUE(HTML5FileWriter::CreateFileForTesting(
      current_browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetFileSystemContext(),
      storage::FileSystemURL::CreateForTest(GURL(download_url)), kPayloadData,
      strlen(kPayloadData)));

  // Now download it.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();

  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("on_record.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string disk_data;
  EXPECT_TRUE(base::ReadFileToString(item->GetTargetFilePath(), &disk_data));
  EXPECT_STREQ(kPayloadData, disk_data.c_str());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_OnDeterminingFilename_NoChange) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  // Wait for the onCreated and onDeterminingFilename events.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(), downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // The download should complete successfully.
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Disabled due to cross-platform flakes; http://crbug.com/370531.
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DISABLED_DownloadExtensionTest_OnDeterminingFilename_Timeout) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  ExtensionDownloadsEventRouter::SetDetermineFilenameTimeoutSecondsForTesting(
      0);

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  // Wait for the onCreated and onDeterminingFilename events.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
      base::StringPrintf("[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(
      downloads::OnDeterminingFilename::kEventName,
      base::StringPrintf("[{\"id\": %d,"
                         "  \"filename\":\"slow.txt\"}]",
                         result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Do not respond to the onDeterminingFilename.

  // The download should complete successfully.
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
      base::StringPrintf("[{\"id\": %d,"
                         "  \"state\": {"
                         "    \"previous\": \"in_progress\","
                         "    \"current\": \"complete\"}}]",
                         result_id)));
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_OnDeterminingFilename_Twice) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  // Wait for the onCreated and onDeterminingFilename events.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
      base::StringPrintf("[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(
      downloads::OnDeterminingFilename::kEventName,
      base::StringPrintf("[{\"id\": %d,"
                         "  \"filename\":\"slow.txt\"}]",
                         result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(), downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // Calling DetermineFilename again should return an error instead of calling
  // DownloadTargetDeterminer.
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("different")),
      downloads::FilenameConflictAction::kOverwrite, &error));
  EXPECT_EQ(errors::kTooManyListeners, error);

  // The download should complete successfully.
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
      base::StringPrintf("[{\"id\": %d,"
                         "  \"state\": {"
                         "    \"previous\": \"in_progress\","
                         "    \"current\": \"complete\"}}]",
                         result_id)));
}

// Tests downloadsInternal.determineFilename.
// Regression test for https://crbug.com/815362.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadsInternalDetermineFilename) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf(R"([{"url": "%s"}])", download_url.c_str()));
  ASSERT_TRUE(result);
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  // Wait for the onCreated and onDeterminingFilename events.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(R"([{
                                               "danger": "safe",
                                               "incognito": false,
                                               "id": %d,
                                               "mime": "text/plain",
                                               "paused": false,
                                               "url": "%s"
                                             }])",
                                         result_id, download_url.c_str())));
  ASSERT_TRUE(
      WaitFor(downloads::OnDeterminingFilename::kEventName,
              base::StringPrintf(
                  R"([{"id": %d, "filename": "slow.txt"}])", result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  std::optional<base::Value> determine_result(RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsInternalDetermineFilenameFunction>(),
      base::StringPrintf(R"([%d, "", "uniquify"])", result_id)));
  EXPECT_FALSE(determine_result);  // No return value.
}

// Tests that overriding a safe file extension to a dangerous extension will not
// trigger the dangerous prompt and will not change the extension.
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_DangerousOverride) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename with a dangerous extension.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("overridden.swf")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf("[{\"id\": %d,"
                                         "  \"state\": {"
                                         "    \"previous\": \"in_progress\","
                                         "    \"current\": \"complete\"}}]",
                                         result_id)));
  EXPECT_EQ(downloads_directory().AppendASCII("overridden.txt"),
            item->GetTargetFilePath());
}

// Tests that overriding a dangerous file extension to a safe extension will
// trigger the dangerous prompt and will not change the extension.
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_SafeOverride) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous =
      safe_browsing::ScopedMarkAllFilesDangerousForTesting();

  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();

  std::string download_url = "data:application/x-shockwave-flash,";
  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(
      downloads::OnCreated::kEventName,
      base::StringPrintf("[{\"danger\": \"safe\","
                         "  \"incognito\": false,"
                         "  \"id\": %d,"
                         "  \"mime\": \"application/x-shockwave-flash\","
                         "  \"paused\": false,"
                         "  \"url\": \"%s\"}]",
                         result_id, download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf("[{\"id\": %d,"
                                         "  \"filename\":\"download.swf\"}]",
                                         result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename with a safe extension.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("overridden.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // Dangerous download prompt will be shown.
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf("[{\"id\": %d, "
                                         "  \"danger\": {"
                                         "    \"previous\": \"safe\","
                                         "    \"current\": \"file\"}}]",
                                         result_id)));

  item->ValidateDangerousDownload();
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"danger\": {"
                          "    \"previous\":\"file\","
                          "    \"current\":\"accepted\"}}]",
                          result_id)));

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
  EXPECT_EQ(downloads_directory().AppendASCII("overridden.swf"),
            item->GetTargetFilePath());
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_ReferencesParentInvalid) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("sneaky/../../sneaky.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_IllegalFilename) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("<")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_IllegalFilenameExtension) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL(
          "My Computer.{20D04FE0-3AEA-1069-A2D8-08002B30309D}/foo")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_ReservedFilename) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("con.foo")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_CurDirInvalid) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL(".")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_ParentDirInvalid) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("..")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());
  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_AbsPathInvalid) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename. Absolute paths should be rejected.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      downloads_directory().Append(FILE_PATH_LITERAL("sneaky.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());

  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// Flaky. crbug.com/1147804
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DISABLED_DownloadExtensionTest_OnDeterminingFilename_EmptyBasenameInvalid) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename. Empty basenames should be rejected.
  std::string error;
  ASSERT_FALSE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("foo/")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_STREQ(errors::kInvalidFilename, error.c_str());

  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// conflictAction may be specified without filename.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_OnDeterminingFilename_Overwrite) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(), downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));

  // Start downloading a file.
  result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller2(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  // Also test that DetermineFilename allows (chrome) extensions to set
  // filenames without (filename) extensions. (Don't ask about v8 extensions or
  // python extensions or kernel extensions or firefox extensions...)
  error = "";
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(), downloads::FilenameConflictAction::kOverwrite, &error));
  EXPECT_EQ("", error);

  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_OnDeterminingFilename_Override) {
  GoOnTheRecord();
  LoadExtension("downloads_split");
  AddFilenameDeterminer();
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(), downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  ASSERT_TRUE(
      WaitFor(downloads::OnChanged::kEventName,
              base::StringPrintf("[{\"id\": %d,"
                                 "  \"filename\": {"
                                 "    \"previous\": \"\","
                                 "    \"current\": \"%s\"}}]",
                                 result_id, GetFilename("slow.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));

  // Start downloading a file.
  result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller2(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  // Also test that DetermineFilename allows (chrome) extensions to set
  // filenames without (filename) extensions. (Don't ask about v8 extensions or
  // python extensions or kernel extensions or firefox extensions...)
  error = "";
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("foo")),
      downloads::FilenameConflictAction::kOverwrite, &error));
  EXPECT_EQ("", error);

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("foo").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// TODO test precedence rules: install_time

#if BUILDFLAG(IS_MAC)
#define MAYBE_DownloadExtensionTest_OnDeterminingFilename_RemoveFilenameDeterminer \
  DISABLED_DownloadExtensionTest_OnDeterminingFilename_RemoveFilenameDeterminer
#else
#define MAYBE_DownloadExtensionTest_OnDeterminingFilename_RemoveFilenameDeterminer \
  DownloadExtensionTest_OnDeterminingFilename_RemoveFilenameDeterminer
#endif
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    MAYBE_DownloadExtensionTest_OnDeterminingFilename_RemoveFilenameDeterminer) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOnTheRecord();
  LoadExtension("downloads_split");
  content::RenderProcessHost* host = AddFilenameDeterminer();
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Remove a determiner while waiting for it.
  RemoveFilenameDeterminer(host);

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// This test is flaky on Linux ASan LSan Tests bot. https://crbug.com/1114226
#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
     defined(ADDRESS_SANITIZER))
#define MAYBE_DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit \
  DISABLED_DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit
#else
#define MAYBE_DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit \
  DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit
#endif
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    MAYBE_DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  GoOnTheRecord();
  AddFilenameDeterminer();

  GoOffTheRecord();
  AddFilenameDeterminer();

  // Start an on-record download.
  GoOnTheRecord();
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_FALSE(GetCurrentManager()->GetBrowserContext()->IsOffTheRecord());
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  // Wait for the onCreated and onDeterminingFilename events.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"incognito\": false,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename events.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("42.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // The download should complete successfully.
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("42.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));

  // Start an incognito download for comparison.
  GoOffTheRecord();
  result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(GetCurrentManager()->GetBrowserContext()->IsOffTheRecord());
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller2(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": true,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  // On-Record renderers should not see events for off-record items.
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"incognito\": true,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  error = "";
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("5.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // The download should complete successfully.
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("5.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DownloadExtensionTest_OnDeterminingFilename_IncognitoSpanning) {
  LoadExtension("downloads_spanning");
  ASSERT_TRUE(StartEmbeddedTestServer());
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  GoOnTheRecord();
  AddFilenameDeterminer();

  // There is a single extension renderer that sees both on-record and
  // off-record events. The extension functions see the on-record profile with
  // include_incognito=true.

  // Start an on-record download.
  GoOnTheRecord();
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_FALSE(GetCurrentManager()->GetBrowserContext()->IsOffTheRecord());
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  // Wait for the onCreated and onDeterminingFilename events.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"incognito\": false,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename events.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), true, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("42.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // The download should complete successfully.
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("42.txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));

  // Start an incognito download for comparison.
  GoOffTheRecord();
  result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  result_id = result->GetInt();
  item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(GetCurrentManager()->GetBrowserContext()->IsOffTheRecord());
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller2(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": true,"
                          "  \"id\": %d,"
                          "  \"mime\": \"text/plain\","
                          "  \"paused\": false,"
                          "  \"url\": \"%s\"}]",
                          result_id,
                          download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"incognito\": true,"
                          "  \"filename\":\"slow.txt\"}]",
                          result_id)));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Respond to the onDeterminingFilename.
  error = "";
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), true, GetExtensionId(), result_id,
      base::FilePath(FILE_PATH_LITERAL("42.txt")),
      downloads::FilenameConflictAction::kUniquify, &error));
  EXPECT_EQ("", error);

  // The download should complete successfully.
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          result_id,
                          GetFilename("42 (1).txt").c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          result_id)));
}

// This test is very flaky on Win XP and Aura. http://crbug.com/248438
// Also flaky on Linux. http://crbug.com/700382
// Also flaky on Mac ASAN.
// Test download interruption while extensions determining filename. Should not
// re-dispatch onDeterminingFilename.
IN_PROC_BROWSER_TEST_F(
    DownloadExtensionTest,
    DISABLED_DownloadExtensionTest_OnDeterminingFilename_InterruptedResume) {
  LoadExtension("downloads_split");
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOnTheRecord();
  content::RenderProcessHost* host = AddFilenameDeterminer();

  // Start a download.
  DownloadItem* item = nullptr;
  {
    DownloadManager* manager = GetCurrentManager();
    std::unique_ptr<content::DownloadTestObserver> observer(
        new JustInProgressDownloadObserver(manager, 1));
    ASSERT_EQ(0, manager->InProgressCount());
    ASSERT_EQ(0, manager->BlockingShutdownCount());
    // Tabs created just for a download are automatically closed, invalidating
    // the download's WebContents. Downloads without WebContents cannot be
    // resumed. http://crbug.com/225901
    ui_test_utils::NavigateToURLWithDisposition(
        current_browser(),
        // This code used to use a mock class that no longer works, due to the
        // NetworkService shipping.
        // TODO(crbug.com/41306723): Fix or delete this test.
        GURL(), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::IN_PROGRESS));
    DownloadManager::DownloadVector items;
    manager->GetAllDownloads(&items);
    for (download::DownloadItem* download_item : items) {
      if (download_item->GetState() == DownloadItem::IN_PROGRESS) {
        // There should be only one IN_PROGRESS item.
        EXPECT_EQ(nullptr, item);
        item = download_item;
      }
    }
    ASSERT_TRUE(item);
  }
  ScopedCancellingItem canceller(item);

  // Wait for the onCreated and onDeterminingFilename event.
  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(
                          "[{\"danger\": \"safe\","
                          "  \"incognito\": false,"
                          "  \"id\": %d,"
                          "  \"mime\": \"application/octet-stream\","
                          "  \"paused\": false}]",
                          item->GetId())));
  ASSERT_TRUE(WaitFor(downloads::OnDeterminingFilename::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"incognito\": false,"
                          "  \"filename\":\"download-unknown-size\"}]",
                          item->GetId())));
  ASSERT_TRUE(item->GetTargetFilePath().empty());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  ClearEvents();
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(),
      // This code used to use a mock class that no longer works, due to the
      // NetworkService shipping.
      // TODO(crbug.com/41306723): Fix or delete this test.
      GURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Errors caught before filename determination are delayed until after
  // filename determination.
  std::string error;
  ASSERT_TRUE(ExtensionDownloadsEventRouter::DetermineFilename(
      current_browser()->profile(), false, GetExtensionId(), item->GetId(),
      base::FilePath(FILE_PATH_LITERAL("42.txt")),
      downloads::FilenameConflictAction::kUniquify, &error))
      << error;
  EXPECT_EQ("", error);
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"filename\": {"
                          "    \"previous\": \"\","
                          "    \"current\": \"%s\"}}]",
                          item->GetId(),
                          GetFilename("42.txt").c_str())));

  content::DownloadUpdatedObserver interrupted(
      item, base::BindRepeating(ItemIsInterrupted));
  ASSERT_TRUE(interrupted.WaitForEvent());
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"error\":{\"current\":\"NETWORK_FAILED\"},"
                          "  \"state\":{"
                          "    \"previous\":\"in_progress\","
                          "    \"current\":\"interrupted\"}}]",
                          item->GetId())));

  ClearEvents();
  // Downloads that are restarted on resumption trigger another download target
  // determination.
  RemoveFilenameDeterminer(host);
  item->Resume(true);

  // Errors caught before filename determination is complete are delayed until
  // after filename determination so that, on resumption, filename determination
  // does not need to be re-done. So, there will not be a second
  // onDeterminingFilename event.

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"error\":{\"previous\":\"NETWORK_FAILED\"},"
                          "  \"state\":{"
                          "    \"previous\":\"interrupted\","
                          "    \"current\":\"in_progress\"}}]",
                          item->GetId())));

  ClearEvents();
  FinishFirstSlowDownloads();

  // The download should complete successfully.
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d,"
                          "  \"state\": {"
                          "    \"previous\": \"in_progress\","
                          "    \"current\": \"complete\"}}]",
                          item->GetId())));
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_SetShelfEnabled) {
  LoadExtension("downloads_split");
  EXPECT_TRUE(RunFunction(
      base::MakeRefCounted<DownloadsSetShelfEnabledFunction>(), "[false]"));
  EXPECT_FALSE(DownloadCoreServiceFactory::GetForBrowserContext(
                   current_browser()->profile())
                   ->IsDownloadUiEnabled());
  EXPECT_TRUE(RunFunction(
      base::MakeRefCounted<DownloadsSetShelfEnabledFunction>(), "[true]"));
  EXPECT_TRUE(DownloadCoreServiceFactory::GetForBrowserContext(
                  current_browser()->profile())
                  ->IsDownloadUiEnabled());
  // TODO(benjhayden) Test that browsers associated with other profiles are not
  // affected.
}

// TODO(benjhayden) Figure out why DisableExtension() does not fire
// OnListenerRemoved.

// TODO(benjhayden) Test that the shelf is shown for download() both with and
// without a WebContents.

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_SetUiOptions) {
  LoadExtension("downloads_split");
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": false}])"));
  EXPECT_FALSE(DownloadCoreServiceFactory::GetForBrowserContext(
                   current_browser()->profile())
                   ->IsDownloadUiEnabled());
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": true}])"));
  EXPECT_TRUE(DownloadCoreServiceFactory::GetForBrowserContext(
                  current_browser()->profile())
                  ->IsDownloadUiEnabled());
}

void OnDangerPromptCreated(DownloadDangerPrompt* prompt) {
  prompt->InvokeActionForTesting(DownloadDangerPrompt::ACCEPT);
}

// TODO(https://crbug.com/40304461): Flaky on Mac debug, failing with a timeout.
#if (BUILDFLAG(IS_MAC) && !defined(NDEBUG))
#define MAYBE_DownloadExtensionTest_AcceptDanger \
  DISABLED_DownloadExtensionTest_AcceptDanger
#else
#define MAYBE_DownloadExtensionTest_AcceptDanger \
  DownloadExtensionTest_AcceptDanger
#endif
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       MAYBE_DownloadExtensionTest_AcceptDanger) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous =
      safe_browsing::ScopedMarkAllFilesDangerousForTesting();

  // Download a file that will be marked dangerous; click the browser action
  // button; the browser action popup will call acceptDanger(); when the
  // DownloadDangerPrompt is created, pretend that the user clicks the Accept
  // button; wait until the download completes.
  LoadExtension("downloads_split");
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      "[{\"url\": \"data:application/x-shockwave-flash,\", \"filename\": "
      "\"dangerous.swf\"}]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(
                          "[{\"id\": %d, "
                          "  \"danger\": {"
                          "    \"previous\": \"safe\","
                          "    \"current\": \"file\"}}]",
                          result_id)));
  ASSERT_TRUE(item->IsDangerous());
  ScopedCancellingItem canceller(item);
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          GetCurrentManager(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE));
  DownloadsAcceptDangerFunction::OnPromptCreatedCallback callback =
      base::BindOnce(&OnDangerPromptCreated);
  DownloadsAcceptDangerFunction::OnPromptCreatedForTesting(
      &callback);
  ExtensionActionTestHelper::Create(current_browser())->Press(GetExtensionId());
  observer->WaitForFinished();
}

// Test that file deletion event is correctly generated after download
// completion.
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       DownloadExtensionTest_DeleteFileAfterCompletion) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GoOnTheRecord();
  LoadExtension("downloads_split");
  std::string download_url = embedded_test_server()->GetURL("/slow?0").spec();

  // Start downloading a file.
  std::optional<base::Value> result = RunFunctionAndReturnResult(
      base::MakeRefCounted<DownloadsDownloadFunction>(),
      base::StringPrintf("[{\"url\": \"%s\"}]", download_url.c_str()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_int());
  int result_id = result->GetInt();
  DownloadItem* item = GetCurrentManager()->GetDownload(result_id);
  ASSERT_TRUE(item);
  ScopedCancellingItem canceller(item);
  ASSERT_EQ(download_url, item->GetOriginalUrl().spec());

  ASSERT_TRUE(WaitFor(downloads::OnCreated::kEventName,
                      base::StringPrintf(R"([{"danger": "safe",)"
                                         R"(  "incognito": false,)"
                                         R"(  "id": %d,)"
                                         R"(  "mime": "text/plain",)"
                                         R"(  "paused": false,)"
                                         R"(  "url": "%s"}])",
                                         result_id, download_url.c_str())));
  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(R"([{"id": %d,)"
                                         R"(  "state": {)"
                                         R"(    "previous": "in_progress",)"
                                         R"(    "current": "complete"}}])",
                                         result_id)));

  item->DeleteFile(base::BindOnce(OnFileDeleted));

  ASSERT_TRUE(WaitFor(downloads::OnChanged::kEventName,
                      base::StringPrintf(R"([{"id": %d,)"
                                         R"(  "exists": {)"
                                         R"(    "previous": true,)"
                                         R"(    "current": false}}])",
                                         result_id)));
}

// The DownloadExtensionBubbleEnabledTest relies on the download surface, which
// ChromeOS_ASH doesn't use (see crbug.com/1323505).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class DownloadExtensionBubbleEnabledTest : public DownloadExtensionTest {
 public:
  DownloadExtensionBubbleEnabledTest() = default;

  DownloadDisplay* GetDownloadToolbarButton() {
    return current_browser()
        ->window()
        ->GetDownloadBubbleUIController()
        ->GetDownloadDisplayController()
        ->download_display_for_testing();
  }
};

IN_PROC_BROWSER_TEST_F(DownloadExtensionBubbleEnabledTest, SetUiOptions) {
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);
  LoadExtension("downloads_split");

  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": true}])"));
  EXPECT_TRUE(GetDownloadToolbarButton()->IsShowing());

  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": false}])"));
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());

  items[0]->Cancel(true);
  // Remain hidden on download updates.
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionBubbleEnabledTest,
                       SetUiOptionsBeforeDownloadStart) {
  LoadExtension("downloads_split");
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": false}])"));
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionBubbleEnabledTest,
                       SetUiOptionsOffTheRecord) {
  LoadExtension("downloads_split");
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": false}])"));
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());

  GoOffTheRecord();
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());

  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": true}])"));
  items[0]->Cancel(true);
  EXPECT_TRUE(GetDownloadToolbarButton()->IsShowing());

  GoOnTheRecord();
  EXPECT_TRUE(GetDownloadToolbarButton()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionBubbleEnabledTest,
                       SetUiOptionsMultipleExtensions) {
  LoadExtension("downloads_split");
  EXPECT_TRUE(RunFunction(base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                          R"([{"enabled": false}])"));
  DownloadManager::DownloadVector items;
  CreateTwoDownloads(&items);
  ScopedItemVectorCanceller delete_items(&items);
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());

  LoadSecondExtension("downloads_spanning");
  // Returns error because the first extension has disabled the UI.
  EXPECT_STREQ(errors::kUiDisabled,
               RunFunctionAndReturnErrorInSecondExtension(
                   base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
                   R"([{"enabled": true}])")
                   .c_str());
  // Two extensions can set the UI to disabled at the same time. No error should
  // be returned.
  EXPECT_TRUE(RunFunctionInSecondExtension(
      base::MakeRefCounted<DownloadsSetUiOptionsFunction>(),
      R"([{"enabled": false}])"));

  DisableExtension(GetExtensionId());
  items[0]->Pause();
  // The UI keeps disabled because the second extension has set it to disabled.
  EXPECT_FALSE(GetDownloadToolbarButton()->IsShowing());

  DisableExtension(GetSecondExtensionId());
  items[0]->Cancel(true);
  EXPECT_TRUE(GetDownloadToolbarButton()->IsShowing());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class DownloadsApiTest : public ExtensionApiTest {
 public:
  DownloadsApiTest() {}

  DownloadsApiTest(const DownloadsApiTest&) = delete;
  DownloadsApiTest& operator=(const DownloadsApiTest&) = delete;

  ~DownloadsApiTest() override {}
};


IN_PROC_BROWSER_TEST_F(DownloadsApiTest, DownloadsApiTest) {
  ASSERT_TRUE(RunExtensionTest("downloads")) << message_;
}

TEST(ExtensionDetermineDownloadFilenameInternal,
     ExtensionDetermineDownloadFilenameInternal) {
  std::string winner_id;
  base::FilePath filename;
  downloads::FilenameConflictAction conflict_action =
      downloads::FilenameConflictAction::kUniquify;
  WarningSet warnings;

  // Empty incumbent determiner
  warnings.clear();
  ExtensionDownloadsEventRouter::DetermineFilenameInternal(
      base::FilePath(FILE_PATH_LITERAL("a")),
      downloads::FilenameConflictAction::kOverwrite, "suggester",
      base::Time::Now(), "", base::Time(), &winner_id, &filename,
      &conflict_action, &warnings);
  EXPECT_EQ("suggester", winner_id);
  EXPECT_EQ(FILE_PATH_LITERAL("a"), filename.value());
  EXPECT_EQ(downloads::FilenameConflictAction::kOverwrite, conflict_action);
  EXPECT_TRUE(warnings.empty());

  // Incumbent wins
  warnings.clear();
  ExtensionDownloadsEventRouter::DetermineFilenameInternal(
      base::FilePath(FILE_PATH_LITERAL("b")),
      downloads::FilenameConflictAction::kPrompt, "suggester",
      base::Time::Now() - base::Days(1), "incumbent", base::Time::Now(),
      &winner_id, &filename, &conflict_action, &warnings);
  EXPECT_EQ("incumbent", winner_id);
  EXPECT_EQ(FILE_PATH_LITERAL("a"), filename.value());
  EXPECT_EQ(downloads::FilenameConflictAction::kOverwrite, conflict_action);
  EXPECT_FALSE(warnings.empty());
  EXPECT_EQ(Warning::kDownloadFilenameConflict,
            warnings.begin()->warning_type());
  EXPECT_EQ("suggester", warnings.begin()->extension_id());

  // Suggester wins
  warnings.clear();
  ExtensionDownloadsEventRouter::DetermineFilenameInternal(
      base::FilePath(FILE_PATH_LITERAL("b")),
      downloads::FilenameConflictAction::kPrompt, "suggester",
      base::Time::Now(), "incumbent", base::Time::Now() - base::Days(1),
      &winner_id, &filename, &conflict_action, &warnings);
  EXPECT_EQ("suggester", winner_id);
  EXPECT_EQ(FILE_PATH_LITERAL("b"), filename.value());
  EXPECT_EQ(downloads::FilenameConflictAction::kPrompt, conflict_action);
  EXPECT_FALSE(warnings.empty());
  EXPECT_EQ(Warning::kDownloadFilenameConflict,
            warnings.begin()->warning_type());
  EXPECT_EQ("incumbent", warnings.begin()->extension_id());
}

}  // namespace extensions
