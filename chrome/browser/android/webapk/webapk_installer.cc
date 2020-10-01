// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include <set>
#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/android/chrome_jni_headers/WebApkInstaller_jni.h"
#include "chrome/browser/android/color_helpers.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk.pb.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The default number of milliseconds to wait for the WebAPK download URL from
// the WebAPK server.
constexpr int kWebApkDownloadUrlTimeoutMs = 60000;

// Limit the icon size to 512KB.
constexpr size_t kMaxIconSizeInBytes = 512 * 1024;

class CacheClearer : public content::BrowsingDataRemover::Observer {
 public:
  ~CacheClearer() override { remover_->RemoveObserver(this); }

  // Clear Chrome's cache. Run |callback| once clearing the cache is complete.
  static void FreeCacheAsync(content::BrowsingDataRemover* remover,
                             base::OnceClosure callback) {
    // CacheClearer manages its own lifetime and deletes itself when finished.
    auto* cache_clearer = new CacheClearer(remover, std::move(callback));
    remover->AddObserver(cache_clearer);
    remover->RemoveAndReply(base::Time(), base::Time::Max(),
                            content::BrowsingDataRemover::DATA_TYPE_CACHE,
                            ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
                            cache_clearer);
  }

 private:
  CacheClearer(content::BrowsingDataRemover* remover,
               base::OnceClosure callback)
      : remover_(remover), install_callback_(std::move(callback)) {}

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    std::move(install_callback_).Run();
    delete this;  // Matches the new in FreeCacheAsync()
  }

  content::BrowsingDataRemover* remover_;

  base::OnceClosure install_callback_;

  DISALLOW_COPY_AND_ASSIGN(CacheClearer);
};

webapk::WebApk_UpdateReason ConvertUpdateReasonToProtoEnum(
    WebApkUpdateReason update_reason) {
  switch (update_reason) {
    case WebApkUpdateReason::NONE:
      return webapk::WebApk::NONE;
    case WebApkUpdateReason::OLD_SHELL_APK:
      return webapk::WebApk::OLD_SHELL_APK;
    case WebApkUpdateReason::PRIMARY_ICON_HASH_DIFFERS:
      return webapk::WebApk::PRIMARY_ICON_HASH_DIFFERS;
    case WebApkUpdateReason::PRIMARY_ICON_MASKABLE_DIFFERS:
      return webapk::WebApk::PRIMARY_ICON_MASKABLE_DIFFERS;
    case WebApkUpdateReason::SPLASH_ICON_HASH_DIFFERS:
      return webapk::WebApk::SPLASH_ICON_HASH_DIFFERS;
    case WebApkUpdateReason::SCOPE_DIFFERS:
      return webapk::WebApk::SCOPE_DIFFERS;
    case WebApkUpdateReason::START_URL_DIFFERS:
      return webapk::WebApk::START_URL_DIFFERS;
    case WebApkUpdateReason::SHORT_NAME_DIFFERS:
      return webapk::WebApk::SHORT_NAME_DIFFERS;
    case WebApkUpdateReason::NAME_DIFFERS:
      return webapk::WebApk::NAME_DIFFERS;
    case WebApkUpdateReason::BACKGROUND_COLOR_DIFFERS:
      return webapk::WebApk::BACKGROUND_COLOR_DIFFERS;
    case WebApkUpdateReason::THEME_COLOR_DIFFERS:
      return webapk::WebApk::THEME_COLOR_DIFFERS;
    case WebApkUpdateReason::ORIENTATION_DIFFERS:
      return webapk::WebApk::ORIENTATION_DIFFERS;
    case WebApkUpdateReason::DISPLAY_MODE_DIFFERS:
      return webapk::WebApk::DISPLAY_MODE_DIFFERS;
    case WebApkUpdateReason::WEB_SHARE_TARGET_DIFFERS:
      return webapk::WebApk::WEB_SHARE_TARGET_DIFFERS;
    case WebApkUpdateReason::MANUALLY_TRIGGERED:
      return webapk::WebApk::MANUALLY_TRIGGERED;
    case WebApkUpdateReason::SHORTCUTS_DIFFER:
      return webapk::WebApk::SHORTCUTS_DIFFER;
  }
}

// Get Chrome's current ABI. It depends on whether Chrome is running as a 32 bit
// app or 64 bit, and the device's cpu architecture as well. Note: please keep
// this function stay in sync with |chromium_android_linker::GetCpuAbi()|.
std::string getCurrentAbi() {
#if defined(__arm__) && defined(__ARM_ARCH_7A__)
  return "armeabi-v7a";
#elif defined(__arm__)
  return "armeabi";
#elif defined(__i386__)
  return "x86";
#elif defined(__mips__)
  return "mips";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "arm64-v8a";
#else
#error "Unsupported target abi"
#endif
}

void SetImageData(webapk::Image* image, const SkBitmap& icon) {
  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &png_bytes);
  image->set_image_data(png_bytes.data(), png_bytes.size());
}

// Populates webapk::WebApk and returns it.
// Must be called on a worker thread because it encodes an SkBitmap.
// The splash icon can be passed either via |icon_url_to_murmur2_hash| or via
// |splash_icon| parameter. |splash_icon| parameter is only used when the
// splash icon URL is unknown.
std::unique_ptr<std::string> BuildProtoInBackground(
    const ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const SkBitmap& splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<std::string, WebApkIconHasher::Icon> icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    WebApkUpdateReason update_reason) {
  std::unique_ptr<webapk::WebApk> webapk(new webapk::WebApk);
  webapk->set_manifest_url(shortcut_info.manifest_url.spec());
  webapk->set_requester_application_package(
      base::android::BuildInfo::GetInstance()->package_name());
  webapk->set_requester_application_version(version_info::GetVersionNumber());
  webapk->set_android_abi(getCurrentAbi());
  webapk->set_package_name(package_name);
  webapk->set_version(version);
  webapk->set_stale_manifest(is_manifest_stale);
  webapk->set_update_reason(ConvertUpdateReasonToProtoEnum(update_reason));

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  web_app_manifest->set_name(base::UTF16ToUTF8(shortcut_info.name));
  web_app_manifest->set_short_name(base::UTF16ToUTF8(shortcut_info.short_name));
  web_app_manifest->set_start_url(shortcut_info.url.spec());
  web_app_manifest->set_orientation(
      blink::WebScreenOrientationLockTypeToString(shortcut_info.orientation));
  web_app_manifest->set_display_mode(
      blink::DisplayModeToString(shortcut_info.display));
  web_app_manifest->set_background_color(
      OptionalSkColorToString(shortcut_info.background_color));
  web_app_manifest->set_theme_color(
      OptionalSkColorToString(shortcut_info.theme_color));

  std::string* scope = web_app_manifest->add_scopes();
  scope->assign(shortcut_info.scope.spec());

  if (shortcut_info.share_target) {
    webapk::ShareTarget* share_target = web_app_manifest->add_share_targets();
    share_target->set_action(shortcut_info.share_target->action.spec());
    if (shortcut_info.share_target->method ==
        blink::Manifest::ShareTarget::Method::kPost) {
      share_target->set_method("POST");
    } else {
      share_target->set_method("GET");
    }
    if (shortcut_info.share_target->enctype ==
        blink::Manifest::ShareTarget::Enctype::kMultipartFormData) {
      share_target->set_enctype("multipart/form-data");
    } else {
      share_target->set_enctype("application/x-www-form-urlencoded");
    }
    webapk::ShareTargetParams* share_target_params =
        share_target->mutable_params();
    share_target_params->set_title(
        base::UTF16ToUTF8(shortcut_info.share_target->params.title));
    share_target_params->set_text(
        base::UTF16ToUTF8(shortcut_info.share_target->params.text));

    for (const ShareTargetParamsFile& share_target_params_file :
         shortcut_info.share_target->params.files) {
      webapk::ShareTargetParamsFile* share_files =
          share_target_params->add_files();
      share_files->set_name(base::UTF16ToUTF8(share_target_params_file.name));
      for (base::string16 mime_type : share_target_params_file.accept) {
        share_files->add_accept(base::UTF16ToUTF8(mime_type));
      }
    }
  }

  if (shortcut_info.best_primary_icon_url.is_empty()) {
    // Update when web manifest is no longer available.
    webapk::Image* best_primary_icon_image = web_app_manifest->add_icons();
    SetImageData(best_primary_icon_image, primary_icon);
    best_primary_icon_image->add_usages(webapk::Image::PRIMARY_ICON);
    if (is_primary_icon_maskable) {
      best_primary_icon_image->add_purposes(webapk::Image::MASKABLE);
    } else {
      best_primary_icon_image->add_purposes(webapk::Image::ANY);
    }

    if (!splash_icon.drawsNothing()) {
      webapk::Image* splash_icon_image = web_app_manifest->add_icons();
      SetImageData(splash_icon_image, splash_icon);
      splash_icon_image->add_usages(webapk::Image::SPLASH_ICON);
      splash_icon_image->add_purposes(webapk::Image::ANY);
    }
  }

  for (const std::string& icon_url : shortcut_info.icon_urls) {
    webapk::Image* image = web_app_manifest->add_icons();
    auto it = icon_url_to_murmur2_hash.find(icon_url);
    image->set_src(icon_url);
    if (it != icon_url_to_murmur2_hash.end())
      image->set_hash(it->second.hash);

    if (icon_url == shortcut_info.best_primary_icon_url.spec()) {
      SetImageData(image, primary_icon);
      image->add_usages(webapk::Image::PRIMARY_ICON);
      if (is_primary_icon_maskable) {
        image->add_purposes(webapk::Image::MASKABLE);
      } else {
        image->add_purposes(webapk::Image::ANY);
      }
    }
    if (icon_url == shortcut_info.splash_image_url.spec()) {
      if (shortcut_info.splash_image_url !=
          shortcut_info.best_primary_icon_url) {
        image->set_image_data(it->second.unsafe_data);
      }
      image->add_usages(webapk::Image::SPLASH_ICON);
      image->add_purposes(webapk::Image::ANY);
    }
  }

  for (const auto& manifest_shortcut_item : shortcut_info.shortcut_items) {
    auto* shortcut_item = web_app_manifest->add_shortcuts();
    shortcut_item->set_name(base::UTF16ToUTF8(manifest_shortcut_item.name));
    shortcut_item->set_short_name(base::UTF16ToUTF8(
        manifest_shortcut_item.short_name.value_or(base::string16())));
    shortcut_item->set_url(manifest_shortcut_item.url.spec());

    for (const auto& manifest_icon : manifest_shortcut_item.icons) {
      auto* shortcut_icon = shortcut_item->add_icons();
      shortcut_icon->set_src(manifest_icon.src.spec());
      auto shortcut_hash_it =
          icon_url_to_murmur2_hash.find(shortcut_icon->src());
      if (shortcut_hash_it != icon_url_to_murmur2_hash.end()) {
        // Don't move the hash to avoid clearing it in case of duplicates.
        shortcut_icon->set_hash(shortcut_hash_it->second.hash);

        if (shortcut_hash_it->second.unsafe_data.size() <=
            kMaxIconSizeInBytes) {
          // Duplicate icons will have an empty |image_data|.
          shortcut_icon->set_image_data(
              std::move(shortcut_hash_it->second.unsafe_data));
        }
      }
    }
  }

  std::unique_ptr<std::string> serialized_proto =
      std::make_unique<std::string>();
  webapk->SerializeToString(serialized_proto.get());
  return serialized_proto;
}

// Builds the WebAPK proto for an update request and stores it to
// |update_request_path|. Returns whether the proto was successfully written to
// disk.
bool StoreUpdateRequestToFileInBackground(
    const base::FilePath& update_request_path,
    const ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const SkBitmap& splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<std::string, WebApkIconHasher::Icon> icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    WebApkUpdateReason update_reason) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<std::string> proto = BuildProtoInBackground(
      shortcut_info, primary_icon, is_primary_icon_maskable, splash_icon,
      package_name, version, std::move(icon_url_to_murmur2_hash),
      is_manifest_stale, update_reason);

  // Create directory if it does not exist.
  base::CreateDirectory(update_request_path.DirName());

  int bytes_written = base::WriteFile(update_request_path,
                                      proto->c_str(),
                                      proto->size());
  return (bytes_written == static_cast<int>(proto->size()));
}

// Reads |file| and returns contents. Must be called on a background thread.
std::unique_ptr<std::string> ReadFileInBackground(const base::FilePath& file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<std::string> update_request = std::make_unique<std::string>();
  base::ReadFileToString(file, update_request.get());
  return update_request;
}

// Returns task runner for running background tasks.
scoped_refptr<base::TaskRunner> GetBackgroundTaskRunner() {
  return base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // anonymous namespace

WebApkInstaller::~WebApkInstaller() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkInstaller_destroy(env, java_ref_);
  java_ref_.Reset();
}

// static
void WebApkInstaller::InstallAsync(content::BrowserContext* context,
                                   const ShortcutInfo& shortcut_info,
                                   const SkBitmap& primary_icon,
                                   bool is_primary_icon_maskable,
                                   FinishCallback finish_callback) {
  // The installer will delete itself when it is done.
  WebApkInstaller* installer = new WebApkInstaller(context);
  installer->InstallAsync(shortcut_info, primary_icon, is_primary_icon_maskable,
                          std::move(finish_callback));
}

// static
void WebApkInstaller::UpdateAsync(content::BrowserContext* context,
                                  const base::FilePath& update_request_path,
                                  FinishCallback finish_callback) {
  // The installer will delete itself when it is done.
  WebApkInstaller* installer = new WebApkInstaller(context);
  installer->UpdateAsync(update_request_path, std::move(finish_callback));
}

// static
void WebApkInstaller::InstallAsyncForTesting(WebApkInstaller* installer,
                                             const ShortcutInfo& shortcut_info,
                                             const SkBitmap& primary_icon,
                                             bool is_primary_icon_maskable,
                                             FinishCallback callback) {
  installer->InstallAsync(shortcut_info, primary_icon, is_primary_icon_maskable,
                          std::move(callback));
}

// static
void WebApkInstaller::UpdateAsyncForTesting(
    WebApkInstaller* installer,
    const base::FilePath& update_request_path,
    FinishCallback finish_callback) {
  installer->UpdateAsync(update_request_path, std::move(finish_callback));
}

void WebApkInstaller::SetTimeoutMs(int timeout_ms) {
  webapk_server_timeout_ms_ = timeout_ms;
}

void WebApkInstaller::OnInstallFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint result) {
  OnResult(static_cast<WebApkInstallResult>(result));
}

// static
void WebApkInstaller::BuildProto(
    const ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const SkBitmap& splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<std::string, WebApkIconHasher::Icon> icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    base::OnceCallback<void(std::unique_ptr<std::string>)> callback) {
  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::BindOnce(&BuildProtoInBackground, shortcut_info, primary_icon,
                     is_primary_icon_maskable, splash_icon, package_name,
                     version, std::move(icon_url_to_murmur2_hash),
                     is_manifest_stale, WebApkUpdateReason::NONE),
      std::move(callback));
}

// static
void WebApkInstaller::StoreUpdateRequestToFile(
    const base::FilePath& update_request_path,
    const ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const SkBitmap& splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<std::string, WebApkIconHasher::Icon> icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    WebApkUpdateReason update_reason,
    base::OnceCallback<void(bool)> callback) {
  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::BindOnce(&StoreUpdateRequestToFileInBackground, update_request_path,
                     shortcut_info, primary_icon, is_primary_icon_maskable,
                     splash_icon, package_name, version,
                     std::move(icon_url_to_murmur2_hash), is_manifest_stale,
                     update_reason),
      std::move(callback));
}

void WebApkInstaller::InstallOrUpdateWebApk(const std::string& package_name,
                                            const std::string& token) {
  webapk_package_ = package_name;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_webapk_package =
      base::android::ConvertUTF8ToJavaString(env, webapk_package_);
  base::android::ScopedJavaLocalRef<jstring> java_title =
      base::android::ConvertUTF16ToJavaString(env, short_name_);
  base::android::ScopedJavaLocalRef<jstring> java_token =
      base::android::ConvertUTF8ToJavaString(env, token);

  if (task_type_ == WebApkInstaller::INSTALL) {
    webapk::TrackRequestTokenDuration(install_duration_timer_->Elapsed(),
                                      package_name);
    base::android::ScopedJavaLocalRef<jobject> java_primary_icon =
        gfx::ConvertToJavaBitmap(&install_primary_icon_);
    Java_WebApkInstaller_installWebApkAsync(
        env, java_ref_, java_webapk_package, webapk_version_, java_title,
        java_token, install_shortcut_info_->source, java_primary_icon);
  } else {
    Java_WebApkInstaller_updateAsync(env, java_ref_, java_webapk_package,
                                     webapk_version_, java_title, java_token);
  }
}

void WebApkInstaller::OnResult(WebApkInstallResult result) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(finish_callback_).Run(result, relax_updates_, webapk_package_);

  if (task_type_ == WebApkInstaller::INSTALL) {
    if (result == WebApkInstallResult::SUCCESS) {
      webapk::TrackInstallDuration(install_duration_timer_->Elapsed());
      webapk::TrackInstallEvent(webapk::INSTALL_COMPLETED);
      WebApkUkmRecorder::RecordInstall(install_shortcut_info_->manifest_url,
                                       webapk_version_);
    } else {
      DVLOG(1) << "The WebAPK installation failed.";
      webapk::TrackInstallEvent(webapk::INSTALL_FAILED);
    }
  }

  delete this;
}

WebApkInstaller::WebApkInstaller(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      webapk_server_timeout_ms_(kWebApkDownloadUrlTimeoutMs),
      webapk_version_(0),
      relax_updates_(false),
      task_type_(UNDEFINED) {
  CreateJavaRef();
  server_url_ = GetServerUrl();
}

void WebApkInstaller::CreateJavaRef() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_WebApkInstaller_create(env, reinterpret_cast<intptr_t>(this)));
}

void WebApkInstaller::InstallAsync(const ShortcutInfo& shortcut_info,
                                   const SkBitmap& primary_icon,
                                   bool is_primary_icon_maskable,
                                   FinishCallback finish_callback) {
  install_duration_timer_.reset(new base::ElapsedTimer());

  install_shortcut_info_.reset(new ShortcutInfo(shortcut_info));
  install_primary_icon_ = primary_icon;
  is_primary_icon_maskable_ = is_primary_icon_maskable;
  short_name_ = shortcut_info.short_name;
  finish_callback_ = std::move(finish_callback);
  task_type_ = INSTALL;

  if (!server_url_.is_valid()) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  CheckFreeSpace();
}

void WebApkInstaller::CheckFreeSpace() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkInstaller_checkFreeSpace(env, java_ref_);
}

void WebApkInstaller::OnGotSpaceStatus(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint status) {
  SpaceStatus space_status = static_cast<SpaceStatus>(status);
  if (space_status == SpaceStatus::NOT_ENOUGH_SPACE) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  if (space_status == SpaceStatus::ENOUGH_SPACE_AFTER_FREE_UP_CACHE) {
    CacheClearer::FreeCacheAsync(
        content::BrowserContext::GetBrowsingDataRemover(browser_context_),
        base::BindOnce(&WebApkInstaller::OnHaveSufficientSpaceForInstall,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnHaveSufficientSpaceForInstall();
  }
}

void WebApkInstaller::UpdateAsync(const base::FilePath& update_request_path,
                                  FinishCallback finish_callback) {
  finish_callback_ = std::move(finish_callback);
  task_type_ = UPDATE;

  if (!server_url_.is_valid()) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::BindOnce(&ReadFileInBackground, update_request_path),
      base::BindOnce(&WebApkInstaller::OnReadUpdateRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnReadUpdateRequest(
    std::unique_ptr<std::string> update_request) {
  std::unique_ptr<webapk::WebApk> proto(new webapk::WebApk);
  if (update_request->empty() || !proto->ParseFromString(*update_request)) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  webapk_package_ = proto->package_name();
  short_name_ = base::UTF8ToUTF16(proto->manifest().short_name());

  SendRequest(std::move(update_request));
}

void WebApkInstaller::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  timer_.Stop();

  int response_code = -1;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();

  if (!response_body || response_code != net::HTTP_OK) {
    LOG(WARNING) << base::StringPrintf(
        "WebAPK server returned response code %d.", response_code);
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  std::unique_ptr<webapk::WebApkResponse> response(new webapk::WebApkResponse);
  if (!response_body || !response->ParseFromString(*response_body)) {
    LOG(WARNING) << "WebAPK server did not return proto.";
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  base::StringToInt(response->version(), &webapk_version_);
  const std::string& token = response->token();
  if (task_type_ == UPDATE && token.empty()) {
    // https://crbug.com/680131. The server sends an empty URL if the server
    // does not have a newer WebAPK to update to.
    relax_updates_ = response->relax_updates();
    OnResult(WebApkInstallResult::SUCCESS);
    return;
  }

  if (token.empty() || response->package_name().empty()) {
    LOG(WARNING) << "WebAPK server returned incomplete proto.";
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  InstallOrUpdateWebApk(response->package_name(), token);
}

network::SharedURLLoaderFactory* GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  return content::BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

void WebApkInstaller::OnHaveSufficientSpaceForInstall() {
  // We need to take the hash of the bitmap at the icon URL prior to any
  // transformations being applied to the bitmap (such as encoding/decoding
  // the bitmap). The icon hash is used to determine whether the icon that
  // the user sees matches the icon of a WebAPK that the WebAPK server
  // generated for another user. (The icon can be dynamically generated.)
  //
  // We redownload the icon in order to take the Murmur2 hash. The redownload
  // should be fast because the icon should be in the HTTP cache.

  std::set<GURL> icons{install_shortcut_info_->best_primary_icon_url};
  if (!install_shortcut_info_->splash_image_url.is_empty() &&
      install_shortcut_info_->splash_image_url !=
          install_shortcut_info_->best_primary_icon_url) {
    icons.insert(install_shortcut_info_->splash_image_url);
  }

  for (const auto& shortcut_icon :
       install_shortcut_info_->best_shortcut_icon_urls) {
    if (shortcut_icon.is_valid())
      icons.insert(shortcut_icon);
  }

  WebApkIconHasher::DownloadAndComputeMurmur2Hash(
      GetURLLoaderFactory(browser_context_),
      url::Origin::Create(install_shortcut_info_->url), icons,
      base::BindOnce(&WebApkInstaller::OnGotIconMurmur2Hashes,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnGotIconMurmur2Hashes(
    base::Optional<std::map<std::string, WebApkIconHasher::Icon>> hashes) {
  if (!hashes) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  // Using empty |splash_icon| here because in this code path (WebApk install),
  // we are using the splash icon data from |hashes|.
  BuildProto(*install_shortcut_info_, install_primary_icon_,
             is_primary_icon_maskable_, SkBitmap() /* splash_icon */,
             "" /* package_name */, "" /* version */, std::move(*hashes),
             false /* is_manifest_stale */,
             base::BindOnce(&WebApkInstaller::SendRequest,
                            weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::SendRequest(
    std::unique_ptr<std::string> serialized_proto) {
  DCHECK(server_url_.is_valid());

  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(webapk_server_timeout_ms_),
      base::BindOnce(&WebApkInstaller::OnResult, weak_ptr_factory_.GetWeakPtr(),
                     WebApkInstallResult::FAILURE));

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = server_url_;
  request->method = "POST";
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  loader_ = network::SimpleURLLoader::Create(std::move(request),
                                             NO_TRAFFIC_ANNOTATION_YET);
  loader_->AttachStringForUpload(*serialized_proto, kProtoMimeType);
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      GetURLLoaderFactory(browser_context_),
      base::BindOnce(&WebApkInstaller::OnURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

GURL WebApkInstaller::GetServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL command_line_url(
      command_line->GetSwitchValueASCII(switches::kWebApkServerUrl));

  if (command_line_url.is_valid())
    return command_line_url;

  JNIEnv* env = base::android::AttachCurrentThread();
  return GURL(base::android::ConvertJavaStringToUTF8(
      env, Java_WebApkInstaller_getWebApkServerUrl(env, java_ref_)));
}
