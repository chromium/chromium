// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeAppInstance : public mojom::AppInstance {
 public:
  enum class IconResponseType {
    // Generate and send good icon.
    ICON_RESPONSE_SEND_GOOD,
    // Generate an empty icon.
    ICON_RESPONSE_SEND_EMPTY,
    // Generate broken bad icon.
    ICON_RESPONSE_SEND_BAD,
    // Don't send icon.
    ICON_RESPONSE_SKIP,
  };
  class Request {
   public:
    Request(const std::string& package_name, const std::string& activity)
        : package_name_(package_name), activity_(activity) {}

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    ~Request() {}

    const std::string& package_name() const { return package_name_; }

    const std::string& activity() const { return activity_; }

    bool IsForApp(const mojom::AppInfo& app_info) const {
      return package_name_ == app_info.package_name &&
             activity_ == app_info.activity;
    }

   private:
    std::string package_name_;
    std::string activity_;
  };

  class IconRequest : public Request {
   public:
    IconRequest(const std::string& package_name,
                const std::string& activity,
                int dimension)
        : Request(package_name, activity),
          dimension_(static_cast<int>(dimension)) {}

    IconRequest(const IconRequest&) = delete;
    IconRequest& operator=(const IconRequest&) = delete;

    ~IconRequest() {}

    int dimension() const { return dimension_; }

   private:
    const int dimension_;
  };

  class ShortcutIconRequest {
   public:
    ShortcutIconRequest(const std::string& icon_resource_id, int dimension)
        : icon_resource_id_(icon_resource_id),
          dimension_(static_cast<int>(dimension)) {}

    ShortcutIconRequest(const ShortcutIconRequest&) = delete;
    ShortcutIconRequest& operator=(const ShortcutIconRequest&) = delete;

    ~ShortcutIconRequest() {}

    const std::string& icon_resource_id() const { return icon_resource_id_; }
    int dimension() const { return dimension_; }

   private:
    const std::string icon_resource_id_;
    const int dimension_;
  };

  explicit FakeAppInstance(mojom::AppHost* app_host);

  FakeAppInstance(const FakeAppInstance&) = delete;
  FakeAppInstance& operator=(const FakeAppInstance&) = delete;

  ~FakeAppInstance() override;

  // mojom::AppInstance overrides:
  void Init(mojo::PendingRemote<mojom::AppHost> host_remote,
            InitCallback callback) override;
  void LaunchAppWithWindowInfo(const std::string& package_name,
                               const std::string& activity,
                               arc::mojom::WindowInfoPtr window_info) override;
  void LaunchAppShortcutItem(const std::string& package_name,
                             const std::string& shortcut_id,
                             int64_t display_id) override;
  void RequestAppIcon(const std::string& package_name,
                      const std::string& activity,
                      int dimension,
                      RequestAppIconCallback callback) override;
  void GetAppIcon(const std::string& package_name,
                  const std::string& activity,
                  int dimension,
                  GetAppIconCallback callback) override;
  void LaunchIntentWithWindowInfo(
      const std::string& intent_uri,
      arc::mojom::WindowInfoPtr window_info) override;
  void UpdateWindowInfo(arc::mojom::WindowInfoPtr window_info) override;
  void RequestShortcutIcon(const std::string& icon_resource_id,
                           int dimension,
                           RequestShortcutIconCallback callback) override;
  void GetAppShortcutIcon(const std::string& icon_resource_id,
                          int dimension,
                          GetAppShortcutIconCallback callback) override;
  void RequestPackageIcon(const std::string& package_name,
                          int dimension,
                          bool normalize,
                          RequestPackageIconCallback callback) override;
  void GetPackageIcon(const std::string& package_name,
                      int dimension,
                      bool normalize,
                      GetPackageIconCallback callback) override;
  void RemoveCachedIcon(const std::string& icon_resource_id) override;
  void UninstallPackage(const std::string& package_name) override;
  void UpdateAppDetails(const std::string& package_name) override;
  void SetTaskActive(int32_t task_id) override;
  void CloseTask(int32_t task_id) override;
  void ShowPackageInfoDeprecated(const std::string& package_name,
                                 const gfx::Rect& dimension_on_screen) override;
  void ShowPackageInfoOnPageDeprecated(
      const std::string& package_name,
      mojom::ShowPackageInfoPage page,
      const gfx::Rect& dimension_on_screen) override;
  void ShowPackageInfoOnPage(const std::string& package_name,
                             mojom::ShowPackageInfoPage page,
                             int64_t display_id) override;
  void SetNotificationsEnabled(const std::string& package_name,
                               bool enabled) override;
  void InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) override;

  void GetAndroidId(GetAndroidIdCallback callback) override;

  void GetRecentAndSuggestedAppsFromPlayStore(
      const std::string& query,
      int32_t max_results,
      GetRecentAndSuggestedAppsFromPlayStoreCallback callback) override;
  void GetAppShortcutGlobalQueryItems(
      const std::string& query,
      int32_t max_results,
      GetAppShortcutGlobalQueryItemsCallback callback) override;
  void GetAppShortcutItems(const std::string& package_name,
                           GetAppShortcutItemsCallback callback) override;
  void StartPaiFlow(StartPaiFlowCallback callback) override;
  void StartFastAppReinstallFlow(
      const std::vector<std::string>& package_names) override;
  void IsInstallable(const std::string& package_name,
                     IsInstallableCallback callback) override;
  void GetAppCategory(const std::string& package_name,
                      GetAppCategoryCallback callback) override;
  void SetAppLocale(const std::string& package_name,
                    const std::string& locale_tag) override;

  // Methods to reply messages.
  void SendRefreshAppList(const std::vector<mojom::AppInfoPtr>& apps);
  void SendAppAdded(const mojom::AppInfo& app);
  void SendPackageAppListRefreshed(const std::string& package_name,
                                   const std::vector<mojom::AppInfoPtr>& apps);
  void SendTaskCreated(int32_t taskId,
                       const mojom::AppInfo& app,
                       const std::string& intent);
  void SendTaskDescription(int32_t taskId,
                           const std::string& label,
                           const std::string& icon_png_data_as_string);
  void SendTaskDestroyed(int32_t taskId);
  void SendInstallShortcut(const mojom::ShortcutInfo& shortcut);
  void SendUninstallShortcut(const std::string& package_name,
                             const std::string& intent_uri);
  void SendInstallShortcuts(const std::vector<mojom::ShortcutInfo>& shortcuts);
  void SendRefreshPackageList(std::vector<mojom::ArcPackageInfoPtr> packages);
  void SendPackageAdded(mojom::ArcPackageInfoPtr package);
  void SendPackageModified(mojom::ArcPackageInfoPtr package);
  void SendPackageUninstalled(const std::string& pacakge_name);

  void SendInstallationStarted(const std::string& package_name);
  void SendInstallationFinished(const std::string& package_name,
                                bool success,
                                bool is_launchable_app = true);
  void SendInstallationProgressChanged(const std::string& package_name,
                                       float progress);
  void SendInstallationActiveChanged(const std::string& package_name,
                                     bool active);

  // Returns latest icon response for particular dimension. Returns true and
  // fill |png_data_as_string| if icon for |dimension| was generated.
  bool GetIconResponse(int dimension, std::string* png_data_as_string);
  // Generates an icon for app or shorcut, determined by |app_icon| and returns:
  //   nullptr if |icon_response_type_| is IconResponseType::ICON_RESPONSE_SKIP.
  //   valid raw icon png data if
  //         |icon_response_type_| is IconResponseType::ICON_RESPONSE_SEND_GOOD.
  //   invalid raw icon png data in |png_data_as_string| if
  //         |icon_response_type_| is IconResponseType::ICON_RESPONSE_SEND_BAD.
  arc::mojom::RawIconPngDataPtr GenerateIconResponse(int dimension,
                                                     bool app_icon);

  int start_pai_request_count() const { return start_pai_request_count_; }

  int start_fast_app_reinstall_request_count() const {
    return start_fast_app_reinstall_request_count_;
  }

  void set_android_id(int64_t android_id) { android_id_ = android_id; }

  void set_icon_response_type(IconResponseType icon_response_type) {
    icon_response_type_ = icon_response_type;
  }

  void set_pai_state_response(mojom::PaiFlowState pai_state_response) {
    pai_state_response_ = pai_state_response;
  }

  int launch_app_shortcut_item_count() const {
    return launch_app_shortcut_item_count_;
  }

  const std::vector<std::unique_ptr<Request>>& launch_requests() const {
    return launch_requests_;
  }

  const std::vector<std::string>& launch_intents() const {
    return launch_intents_;
  }

  const std::vector<std::unique_ptr<IconRequest>>& icon_requests() const {
    return icon_requests_;
  }

  const std::vector<std::unique_ptr<ShortcutIconRequest>>&
  shortcut_icon_requests() const {
    return shortcut_icon_requests_;
  }

  void set_is_installable(bool is_installable) {
    is_installable_ = is_installable;
  }

  void set_app_category_of_pkg(
      std::string_view pkg_name, mojom::AppCategory category) {
    pkg_name_to_app_category_[std::string(pkg_name)] = category;
  }

  const std::map<std::string, std::string>& selected_locales() const {
    return selected_locales_;
  }
  std::string selected_locale(const std::string& package_name) {
    return selected_locales_[package_name];
  }

 private:
  using TaskIdToInfo = std::map<int32_t, std::unique_ptr<Request>>;

  arc::mojom::RawIconPngDataPtr GetFakeIcon(mojom::ScaleFactor scale_factor);

  // Mojo endpoints.
  raw_ptr<mojom::AppHost, DanglingUntriaged> app_host_;
  // Number of requests to start PAI flows.
  int start_pai_request_count_ = 0;
  // Response for PAI flow state;
  mojom::PaiFlowState pai_state_response_ = mojom::PaiFlowState::SUCCEEDED;
  // Number of requests to start Fast App Reinstall flows.
  int start_fast_app_reinstall_request_count_ = 0;
  // Keeps information about launch app shortcut requests.
  int launch_app_shortcut_item_count_ = 0;
  // AndroidId to return.
  int64_t android_id_ = 0;

  // Keeps information about launch requests.
  std::vector<std::unique_ptr<Request>> launch_requests_;
  // Keeps information about launch intents.
  std::vector<std::string> launch_intents_;
  // Keeps information about icon load requests.
  std::vector<std::unique_ptr<IconRequest>> icon_requests_;
  // Keeps information about shortcut icon load requests.
  std::vector<std::unique_ptr<ShortcutIconRequest>> shortcut_icon_requests_;
  // Defines how to response to icon requests.
  IconResponseType icon_response_type_ =
      IconResponseType::ICON_RESPONSE_SEND_GOOD;
  // Keeps latest generated icons per icon dimension.
  std::map<int, std::string> icon_responses_;
  // Stores information for serving GetAppCategory calls.
  std::map<std::string, mojom::AppCategory> pkg_name_to_app_category_;

  bool is_installable_ = false;

  std::map<std::string, std::string> selected_locales_;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojo::Remote<mojom::AppHost> host_remote_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
