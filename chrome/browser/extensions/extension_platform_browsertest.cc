// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_platform_browsertest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_paths.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#else
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/extensions/platform_test_extension_loader.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#endif

namespace extensions {
namespace {

using ContextType = extensions::browser_test_util::ContextType;

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  NotificationDisplayServiceTester::EnsureFactoryBuilt();
}

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// ActivityType that doesn't restore tabs on cold start. Any type other than
// kTabbed is fine.
const auto kTestActivityType = chrome::android::ActivityType::kCustomTab;

bool IsMV3AllowedContextType(ContextType context_type) {
  return context_type == ContextType::kServiceWorker ||
         context_type == ContextType::kFromManifest ||
         context_type == ContextType::kNone;
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

}  // namespace

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// TestTabModel provides a means of creating a tab associated with a given
// profile. The new tab can then be added to Android's TabModelList.
class ExtensionPlatformBrowserTest::TestTabModel : public TabModel {
 public:
  explicit TestTabModel(Profile* profile)
      : TabModel(profile, kTestActivityType),
        web_contents_(content::WebContents::Create(
            content::WebContents::CreateParams(GetProfile()))) {}

  ~TestTabModel() override = default;

  // TabModel:
  int GetTabCount() const override { return 0; }
  int GetActiveIndex() const override { return 0; }
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override {
    return nullptr;
  }
  content::WebContents* GetActiveWebContents() const override {
    return web_contents_.get();
  }
  content::WebContents* GetWebContentsAt(int index) const override {
    return nullptr;
  }
  TabAndroid* GetTabAt(int index) const override { return nullptr; }
  void SetActiveIndex(int index) override {}
  void ForceCloseAllTabs() override {}
  void CloseTabAt(int index) override {}
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents,
                 bool select) override {}
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override {}
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override {
    return nullptr;
  }
  bool IsSessionRestoreInProgress() const override { return false; }
  bool IsActiveModel() const override { return false; }
  void AddObserver(TabModelObserver* observer) override {}
  void RemoveObserver(TabModelObserver* observer) override {}
  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override {
    return 0;
  }
  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override {}

 private:
  // The WebContents associated with this tab's profile.
  std::unique_ptr<content::WebContents> web_contents_;
};
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

ExtensionPlatformBrowserTest::ExtensionPlatformBrowserTest(
    ContextType context_type)
    : context_type_(context_type) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // Android only allows certain context types.
  EXPECT_TRUE(IsMV3AllowedContextType(context_type));
#endif
}

ExtensionPlatformBrowserTest::~ExtensionPlatformBrowserTest() = default;

void ExtensionPlatformBrowserTest::SetUp() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
  PlatformBrowserTest::SetUp();
}

void ExtensionPlatformBrowserTest::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();

  RegisterPathProvider();
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

  web_contents_ = GetActiveWebContents()->GetWeakPtr();
}

void ExtensionPlatformBrowserTest::TearDown() {
  web_contents_.reset();
  PlatformBrowserTest::TearDown();
}

void ExtensionPlatformBrowserTest::TearDownOnMainThread() {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  if (tab_model_) {
    TabModelList::RemoveTabModel(tab_model_.get());
    tab_model_.reset();
  }
#endif
  PlatformBrowserTest::TearDownOnMainThread();
}

const Extension* ExtensionPlatformBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtension(path, {});
}

const Extension* ExtensionPlatformBrowserTest::LoadExtension(
    const base::FilePath& path,
    const LoadOptions& options) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  base::FilePath extension_path;
  if (!extensions::browser_test_util::ModifyExtensionIfNeeded(
          options, context_type_, GetTestPreCount(), temp_dir_.GetPath(), path,
          &extension_path)) {
    return nullptr;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeTestExtensionLoader loader(profile());
#else
  PlatformTestExtensionLoader loader(profile());
#endif
  loader.set_allow_incognito_access(options.allow_in_incognito);
  loader.set_allow_file_access(options.allow_file_access);
  loader.set_ignore_manifest_warnings(options.ignore_manifest_warnings);
  loader.set_wait_for_renderers(options.wait_for_renderers);

  if (options.install_param != nullptr) {
    loader.set_install_param(options.install_param);
  }

  // TODO(crbug.com/373434594): Support TestServiceWorkerContextObserver for the
  // wait_for_registration_stored option.
  CHECK(!options.wait_for_registration_stored);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(extension_path);
  last_loaded_extension_id_ = extension->id();
  return extension.get();
}

void ExtensionPlatformBrowserTest::DisableExtension(
    const std::string& extension_id,
    int disable_reasons) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionSystem::Get(profile())->extension_service()->DisableExtension(
      extension_id, disable_reasons);
#else
  DesktopAndroidExtensionSystem* extension_system =
      static_cast<DesktopAndroidExtensionSystem*>(
          ExtensionSystem::Get(profile()));
  ASSERT_TRUE(extension_system);
  extension_system->DisableExtension(extension_id, disable_reasons);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

content::WebContents* ExtensionPlatformBrowserTest::GetActiveWebContents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

Profile* ExtensionPlatformBrowserTest::GetOrCreateIncognitoProfile() {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // Ensure ExtensionSystem is properly initialized for `incognito_profile`
  // in split mode.
  // TODO(crbug.com/356905053): Remove this workaround when the proper
  // extension runtime is implemented on Android.
  util::InitExtensionSystemForIncognitoSplit(incognito_profile);
#endif

  return incognito_profile;
}

void ExtensionPlatformBrowserTest::PlatformOpenURLOffTheRecord(
    Profile* profile,
    const GURL& url) {
#if !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  OpenURLOffTheRecord(profile, url);
#else
  // Android doesn't have an OpenURLOffTheRecord() helper so we roll our own.
  Profile* incognito_profile =
      this->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  if (tab_model_) {
    TabModelList::RemoveTabModel(tab_model_.get());
    tab_model_.reset();
  }
  // Create a tab model for the incognito profile then navigate to the URL.
  tab_model_ = std::make_unique<TestTabModel>(incognito_profile);
  TabModelList::AddTabModel(tab_model_.get());
  // This blocks until the navigation completes.
  ASSERT_TRUE(content::NavigateToURL(tab_model_->GetActiveWebContents(), url));
#endif
}

Profile* ExtensionPlatformBrowserTest::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* ExtensionPlatformBrowserTest::web_contents() {
  return web_contents_.get();
}

}  // namespace extensions
