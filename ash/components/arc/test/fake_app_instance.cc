// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_app_instance.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/app/arc_playstore_search_request_state.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"

namespace {

void FillRawIconPngData(const std::string& icon_png_data_as_string,
                        arc::mojom::RawIconPngData* icon) {
  icon->is_adaptive_icon = true;
  icon->icon_png_data = std::vector<uint8_t>(icon_png_data_as_string.begin(),
                                             icon_png_data_as_string.end());
  icon->foreground_icon_png_data = std::vector<uint8_t>(
      icon_png_data_as_string.begin(), icon_png_data_as_string.end());
  icon->background_icon_png_data = std::vector<uint8_t>(
      icon_png_data_as_string.begin(), icon_png_data_as_string.end());
}

}  // namespace

namespace mojo {

template <>
struct TypeConverter<arc::mojom::AppInfoPtr, arc::mojom::AppInfo> {
  static arc::mojom::AppInfoPtr Convert(const arc::mojom::AppInfo& app_info) {
    return app_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace arc {

FakeAppInstance::FakeAppInstance(mojom::AppHost* app_host)
    : app_host_(app_host) {}
FakeAppInstance::~FakeAppInstance() {}

void FakeAppInstance::Init(mojo::PendingRemote<mojom::AppHost> host_remote,
                           InitCallback callback) {
  // For every change in a connection bind latest remote.
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakeAppInstance::LaunchAppWithWindowInfo(
    const std::string& package_name,
    const std::string& activity,
    arc::mojom::WindowInfoPtr window_info) {
  launch_requests_.push_back(std::make_unique<Request>(package_name, activity));
}

void FakeAppInstance::LaunchAppShortcutItem(const std::string& package_name,
                                            const std::string& shortcut_id,
                                            int64_t display_id) {
  ++launch_app_shortcut_item_count_;
}

void FakeAppInstance::RequestAppIcon(const std::string& package_name,
                                     const std::string& activity,
                                     int dimension,
                                     RequestAppIconCallback callback) {
  NOTREACHED();
}

void FakeAppInstance::GetAppIcon(const std::string& package_name,
                                 const std::string& activity,
                                 int dimension,
                                 GetAppIconCallback callback) {
  icon_requests_.push_back(
      std::make_unique<IconRequest>(package_name, activity, dimension));

  auto icon = GenerateIconResponse(dimension, true /* app_icon */);
  std::move(callback).Run(std::move(icon));
}

void FakeAppInstance::SendRefreshAppList(
    const std::vector<mojom::AppInfoPtr>& apps) {
  std::vector<mojom::AppInfoPtr> v;
  for (const auto& app : apps)
    v.emplace_back(app->Clone());
  app_host_->OnAppListRefreshed(std::move(v));
}

void FakeAppInstance::SendPackageAppListRefreshed(
    const std::string& package_name,
    const std::vector<mojom::AppInfoPtr>& apps) {
  std::vector<mojom::AppInfoPtr> v;
  for (const auto& app : apps)
    v.emplace_back(app->Clone());
  app_host_->OnPackageAppListRefreshed(package_name, std::move(v));
}

void FakeAppInstance::SendInstallShortcuts(
    const std::vector<mojom::ShortcutInfo>& shortcuts) {
  for (auto& shortcut : shortcuts)
    SendInstallShortcut(shortcut);
}

void FakeAppInstance::SendInstallShortcut(const mojom::ShortcutInfo& shortcut) {
  app_host_->OnInstallShortcut(shortcut.Clone());
}

void FakeAppInstance::SendUninstallShortcut(const std::string& package_name,
                                            const std::string& intent_uri) {
  app_host_->OnUninstallShortcut(package_name, intent_uri);
}

void FakeAppInstance::SendAppAdded(const mojom::AppInfo& app) {
  app_host_->OnAppAddedDeprecated(mojom::AppInfo::From(app));
}

void FakeAppInstance::SendTaskCreated(int32_t taskId,
                                      const mojom::AppInfo& app,
                                      const std::string& intent) {
  app_host_->OnTaskCreated(taskId, app.package_name, app.activity, app.name,
                           intent, 0 /* session_id */);
}

void FakeAppInstance::SendTaskDescription(
    int32_t taskId,
    const std::string& label,
    const std::string& icon_png_data_as_string) {
  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New();
  FillRawIconPngData(icon_png_data_as_string, icon.get());
  app_host_->OnTaskDescriptionChanged(taskId, label, std::move(icon), 0, 0);
}

void FakeAppInstance::SendTaskDestroyed(int32_t taskId) {
  app_host_->OnTaskDestroyed(taskId);
}

bool FakeAppInstance::GetIconResponse(int dimension,
                                      std::string* png_data_as_string) {
  const auto previous_response = icon_responses_.find(dimension);
  if (previous_response == icon_responses_.end())
    return false;
  *png_data_as_string = previous_response->second;
  return true;
}

arc::mojom::RawIconPngDataPtr FakeAppInstance::GenerateIconResponse(
    int dimension,
    bool app_icon) {
  auto previous_response = icon_responses_.find(dimension);
  if (previous_response != icon_responses_.end())
    icon_responses_.erase(previous_response);

  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New();
  switch (icon_response_type_) {
    case IconResponseType::ICON_RESPONSE_SKIP:
      return nullptr;
    case IconResponseType::ICON_RESPONSE_SEND_BAD: {
      std::string bad_png_data_as_string = "BAD_ICON_CONTENT";
      FillRawIconPngData(bad_png_data_as_string, icon.get());
      icon_responses_[dimension] = bad_png_data_as_string;
      return icon;
    }
    case IconResponseType::ICON_RESPONSE_SEND_EMPTY: {
      FillRawIconPngData(std::string(), icon.get());
      icon_responses_[dimension] = std::string();
      return icon;
    }
    case IconResponseType::ICON_RESPONSE_SEND_GOOD: {
      base::FilePath base_path;
      CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
      base::FilePath icon_file_path =
          base_path.AppendASCII("ash")
              .AppendASCII("components")
              .AppendASCII("arc")
              .AppendASCII("test")
              .AppendASCII("data")
              .AppendASCII("icons")
              .AppendASCII(base::StringPrintf(
                  "icon_%s_%d.png", app_icon ? "app" : "shortcut", dimension));
      std::string good_png_data_as_string;
      {
        base::ScopedAllowBlockingForTesting allow_io;
        CHECK(base::PathExists(icon_file_path))
            << icon_file_path.MaybeAsASCII();
        CHECK(base::ReadFileToString(icon_file_path, &good_png_data_as_string));
      }
      FillRawIconPngData(good_png_data_as_string, icon.get());
      icon_responses_[dimension] = good_png_data_as_string;
      return icon;
    }
  }
}

arc::mojom::RawIconPngDataPtr FakeAppInstance::GetFakeIcon(
    mojom::ScaleFactor scale_factor) {
  std::string icon_file_name;
  switch (scale_factor) {
    case mojom::ScaleFactor::SCALE_FACTOR_100P:
      icon_file_name = "icon_100p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_125P:
      icon_file_name = "icon_125p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_133P:
      icon_file_name = "icon_133p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_140P:
      icon_file_name = "icon_140p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_150P:
      icon_file_name = "icon_150p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_180P:
      icon_file_name = "icon_180p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_200P:
      icon_file_name = "icon_200p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_250P:
      icon_file_name = "icon_250p.png";
      break;
    case mojom::ScaleFactor::SCALE_FACTOR_300P:
      icon_file_name = "icon_300p.png";
      break;
    default:
      NOTREACHED();
  }

  base::FilePath base_path;
  std::string png_data_as_string;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
  base::FilePath icon_file_path = base_path.AppendASCII("ash")
                                      .AppendASCII("components")
                                      .AppendASCII("arc")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("icons")
                                      .AppendASCII(icon_file_name);
  CHECK(base::PathExists(icon_file_path));
  CHECK(base::ReadFileToString(icon_file_path, &png_data_as_string));

  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New();
  FillRawIconPngData(png_data_as_string, icon.get());
  return icon;
}

void FakeAppInstance::SendRefreshPackageList(
    std::vector<mojom::ArcPackageInfoPtr> packages) {
  app_host_->OnPackageListRefreshed(std::move(packages));
}

void FakeAppInstance::SendPackageAdded(mojom::ArcPackageInfoPtr package) {
  app_host_->OnPackageAdded(std::move(package));
}

void FakeAppInstance::SendPackageModified(mojom::ArcPackageInfoPtr package) {
  app_host_->OnPackageModified(std::move(package));
}

void FakeAppInstance::SendPackageUninstalled(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::SendInstallationStarted(const std::string& package_name) {
  app_host_->OnInstallationStarted(package_name);
}

void FakeAppInstance::SendInstallationFinished(const std::string& package_name,
                                               bool success,
                                               bool is_launchable_app) {
  mojom::InstallationResult result;
  result.package_name = package_name;
  result.success = success;
  result.is_launchable_app = is_launchable_app;
  app_host_->OnInstallationFinished(
      mojom::InstallationResultPtr(result.Clone()));
}

void FakeAppInstance::UninstallPackage(const std::string& package_name) {
  app_host_->OnPackageRemoved(package_name);
}

void FakeAppInstance::UpdateAppDetails(const std::string& package_name) {}

void FakeAppInstance::SetTaskActive(int32_t task_id) {}

void FakeAppInstance::CloseTask(int32_t task_id) {}

void FakeAppInstance::ShowPackageInfoDeprecated(
    const std::string& package_name,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::ShowPackageInfoOnPageDeprecated(
    const std::string& package_name,
    mojom::ShowPackageInfoPage page,
    const gfx::Rect& dimension_on_screen) {}

void FakeAppInstance::ShowPackageInfoOnPage(const std::string& package_name,
                                            mojom::ShowPackageInfoPage page,
                                            int64_t display_id) {}

void FakeAppInstance::SetNotificationsEnabled(const std::string& package_name,
                                              bool enabled) {}

void FakeAppInstance::InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) {
  app_host_->OnPackageAdded(std::move(arcPackageInfo));
}

void FakeAppInstance::GetAndroidId(GetAndroidIdCallback callback) {
  std::move(callback).Run(android_id_);
}

void FakeAppInstance::GetRecentAndSuggestedAppsFromPlayStore(
    const std::string& query,
    int32_t max_results,
    GetRecentAndSuggestedAppsFromPlayStoreCallback callback) {
  // Fake Play Store app info
  std::vector<arc::mojom::AppDiscoveryResultPtr> fake_apps;

  // Check if we're fabricating failed query.
  const std::string kFailedQueryPrefix("FailedQueryWithCode-");
  ArcPlayStoreSearchRequestState state_code =
      ArcPlayStoreSearchRequestState::SUCCESS;
  if (!query.compare(0, kFailedQueryPrefix.size(), kFailedQueryPrefix)) {
    state_code = static_cast<ArcPlayStoreSearchRequestState>(
        stoi(query.substr(kFailedQueryPrefix.size())));
    std::move(callback).Run(state_code, std::move(fake_apps));
    return;
  }

  const std::string kPartiallyFailedQueryPrefix(
      "PartiallyFailedQueryWithCode-");
  if (!query.compare(0, kPartiallyFailedQueryPrefix.size(),
                     kPartiallyFailedQueryPrefix)) {
    state_code = static_cast<ArcPlayStoreSearchRequestState>(
        stoi(query.substr(kPartiallyFailedQueryPrefix.size())));
    DCHECK_EQ(state_code,
              ArcPlayStoreSearchRequestState::PHONESKY_RESULT_INVALID_DATA);
  }

  const bool has_price_and_rating = query != "QueryWithoutRatingAndPrice";
  {
    auto icon = GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P);
    const auto& fake_icon_png_data = (!icon || !icon->icon_png_data)
                                         ? std::vector<uint8_t>()
                                         : icon->icon_png_data.value();
    fake_apps.push_back(mojom::AppDiscoveryResult::New(
        std::string("LauncherIntentUri"),  // launch_intent_uri
        std::string("InstallIntentUri"),   // install_intent_uri
        std::string(query),                // label
        false,                             // is_instant_app
        false,                             // is_recent
        std::string("Publisher"),          // publisher_name
        has_price_and_rating ? std::string("$7.22")
                             : std::string(),  // formatted_price
        has_price_and_rating ? 5 : -1,         // review_score
        fake_icon_png_data,                    // icon_png_data
        std::string("com.google.android.gm"),  // package_name
        std::move(icon)));                     // icon
  }

  const int num_results =
      (state_code == ArcPlayStoreSearchRequestState::SUCCESS) ? max_results
                                                              : max_results / 2;
  for (int i = 0; i < num_results - 1; ++i) {
    const bool has_icon =
        query != "QueryWithSomeResultsMissingIcon" || i < num_results / 2;
    auto icon =
        has_icon ? GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P) : nullptr;
    const auto& fake_icon_png_data = (!icon || !icon->icon_png_data)
                                         ? std::vector<uint8_t>()
                                         : icon->icon_png_data.value();
    fake_apps.push_back(mojom::AppDiscoveryResult::New(
        base::StringPrintf("LauncherIntentUri %d", i),  // launch_intent_uri
        base::StringPrintf("InstallIntentUri %d", i),   // install_intent_uri
        base::StringPrintf("%s %d", query.c_str(), i),  // label
        i % 2 == 0,                                     // is_instant_app
        i % 4 == 0,                                     // is_recent
        base::StringPrintf("Publisher %d", i),          // publisher_name
        has_price_and_rating ? base::StringPrintf("$%d.22", i)
                             : std::string(),      // formatted_price
        has_price_and_rating ? i : -1,             // review_score
        fake_icon_png_data,                        // icon_png_data
        base::StringPrintf("test.package.%d", i),  // package_name
        std::move(icon)));                         // icon
  }

  std::move(callback).Run(state_code, std::move(fake_apps));
}

void FakeAppInstance::GetAppShortcutGlobalQueryItems(
    const std::string& query,
    int32_t max_results,
    GetAppShortcutGlobalQueryItemsCallback callback) {
  // Fake app shortcut items results.
  std::vector<mojom::AppShortcutItemPtr> fake_app_shortcut_items;

  for (int i = 0; i < max_results; ++i) {
    auto icon = GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P);
    const auto& fake_icon_png_data = (!icon || !icon->icon_png_data)
                                         ? std::vector<uint8_t>()
                                         : icon->icon_png_data.value();
    fake_app_shortcut_items.emplace_back(mojom::AppShortcutItem::New(
        base::StringPrintf("ShortcutId %d", i),
        base::StringPrintf("ShortLabel %d", i), fake_icon_png_data,
        "FakeAppPackageName", mojom::AppShortcutItemType::kStatic, i,
        std::move(icon)));
  }

  std::move(callback).Run(std::move(fake_app_shortcut_items));
}

void FakeAppInstance::GetAppShortcutItems(
    const std::string& package_name,
    GetAppShortcutItemsCallback callback) {
  // Fake app shortcut items results.
  std::vector<mojom::AppShortcutItemPtr> fake_app_shortcut_items;

  for (int i = 0; i < 3; ++i) {
    auto icon = GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P);
    const auto& fake_icon_png_data = (!icon || !icon->icon_png_data)
                                         ? std::vector<uint8_t>()
                                         : icon->icon_png_data.value();
    fake_app_shortcut_items.push_back(mojom::AppShortcutItem::New(
        base::StringPrintf("ShortcutId %d", i),
        base::StringPrintf("ShortLabel %d", i), fake_icon_png_data,
        package_name, mojom::AppShortcutItemType::kStatic, i, std::move(icon)));
  }

  std::move(callback).Run(std::move(fake_app_shortcut_items));
}

void FakeAppInstance::StartPaiFlow(StartPaiFlowCallback callback) {
  ++start_pai_request_count_;
  std::move(callback).Run(pai_state_response_);
}

void FakeAppInstance::StartFastAppReinstallFlow(
    const std::vector<std::string>& package_names) {
  ++start_fast_app_reinstall_request_count_;
}

void FakeAppInstance::IsInstallable(const std::string& package_name,
                                    IsInstallableCallback callback) {
  std::move(callback).Run(is_installable_);
}

void FakeAppInstance::GetAppCategory(const std::string& package_name,
                                     GetAppCategoryCallback callback) {
  auto itr = pkg_name_to_app_category_.find(package_name);
  auto category = mojom::AppCategory::kUndefined;

  if (itr != pkg_name_to_app_category_.end()) category = itr->second;
  std::move(callback).Run(category);
}

void FakeAppInstance::SetAppLocale(const std::string& package_name,
                                   const std::string& locale_tag) {
  selected_locales_[package_name] = locale_tag;
}

void FakeAppInstance::LaunchIntentWithWindowInfo(
    const std::string& intent_uri,
    arc::mojom::WindowInfoPtr window_info) {
  launch_intents_.push_back(intent_uri);
}

void FakeAppInstance::UpdateWindowInfo(arc::mojom::WindowInfoPtr window_info) {}

void FakeAppInstance::RequestShortcutIcon(
    const std::string& icon_resource_id,
    int dimension,
    RequestShortcutIconCallback callback) {
  NOTREACHED();
}

void FakeAppInstance::GetAppShortcutIcon(const std::string& icon_resource_id,
                                         int dimension,
                                         GetAppShortcutIconCallback callback) {
  shortcut_icon_requests_.push_back(
      std::make_unique<ShortcutIconRequest>(icon_resource_id, dimension));

  auto icon = GenerateIconResponse(dimension, false /* app_icon */);
  std::move(callback).Run(std::move(icon));
}

void FakeAppInstance::RequestPackageIcon(const std::string& package_name,
                                         int dimension,
                                         bool normalize,
                                         RequestPackageIconCallback callback) {
  NOTREACHED();
}

void FakeAppInstance::GetPackageIcon(const std::string& package_name,
                                     int dimension,
                                     bool normalize,
                                     GetPackageIconCallback callback) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::move(callback).Run(GetFakeIcon(mojom::ScaleFactor::SCALE_FACTOR_100P));
}

void FakeAppInstance::RemoveCachedIcon(const std::string& icon_resource_id) {}

void FakeAppInstance::SendInstallationProgressChanged(
    const std::string& package_name,
    float progress) {
  app_host_->OnInstallationProgressChanged(package_name, progress);
}

void FakeAppInstance::SendInstallationActiveChanged(
    const std::string& package_name,
    bool active) {
  app_host_->OnInstallationActiveChanged(package_name, active);
}

}  // namespace arc
