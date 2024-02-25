// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_menu_icon_loader.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/favicon_size.h"
#include "url/origin.h"

namespace controlled_frame {

namespace {
constexpr base::StringPiece kManifestPath =
    "/.well-known/_generated_install_page.html";
constexpr base::StringPiece kIconPath = "/icon.png";
const int kTestWebViewInstanceId = 1;
}  // namespace

class ControlledFrameMenuIconLoaderTest : public WebAppTest {
 public:
  ControlledFrameMenuIconLoaderTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebApps,
                              features::kIsolatedWebAppDevMode},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    WebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    web_app::IsolatedWebAppUrlInfo url_info =
        CreateIsolatedWebApp(kDevAppOriginUrl);
    NavigateAndCommit(kDevAppOriginUrl);
  }

  web_app::IsolatedWebAppUrlInfo CreateIsolatedWebApp(const GURL& url) {
    const base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        web_app::IsolatedWebAppUrlInfo::Create(url);
    CHECK(url_info.has_value());
    SetUpPageAndIconStates(*url_info);
    base::test::TestFuture<
        base::expected<web_app::InstallIsolatedWebAppCommandSuccess,
                       web_app::InstallIsolatedWebAppCommandError>>
        future;
    fake_provider().scheduler().InstallIsolatedWebApp(
        *url_info,
        web_app::DevModeProxy{.proxy_url = url::Origin::Create(GURL(url))},
        /*expected_version=*/base::Version("1.0.0"),
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    auto install_result = future.Take();
    EXPECT_TRUE(install_result.has_value()) << install_result.error();
    return *url_info;
  }

  void SetUpPageAndIconStates(const web_app::IsolatedWebAppUrlInfo& url_info) {
    GURL application_url = url_info.origin().GetURL();
    auto& page_state = web_contents_manager().GetOrCreatePageState(
        application_url.Resolve(kManifestPath));
    page_state.url_load_result = web_app::WebAppUrlLoader::Result::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    page_state.manifest_url = CreateDefaultManifestURL(application_url);
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest = CreateDefaultManifest(application_url);

    auto& icon_state = web_contents_manager().GetOrCreateIconState(
        application_url.Resolve(kIconPath));
    icon_state.bitmaps = {
        web_app::CreateSquareIcon(gfx::kFaviconSize, SK_ColorRED)};
  }

  // Creates and returns a menu manager.
  extensions::MenuManager* CreateMenuManager() {
    return static_cast<extensions::MenuManager*>(
        extensions::MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&extensions::MenuManagerFactory::
                                    BuildServiceInstanceForTesting)));
  }

  // Returns a test item with the given string ID for Controlled Frame.
  std::unique_ptr<extensions::MenuItem> CreateTestItem(
      int webview_embedder_process_id,
      int webview_embedder_frame_id,
      int webview_instance_id,
      const std::string& string_id,
      bool visible) {
    extensions::MenuItem::Id id(
        false, extensions::MenuItem::ExtensionKey(
                   /*extension_id=*/"", webview_embedder_process_id,
                   webview_embedder_frame_id, webview_instance_id));
    id.string_uid = string_id;
    return std::make_unique<extensions::MenuItem>(
        id, "test", false, visible, true, extensions::MenuItem::NORMAL,
        extensions::MenuItem::ContextList(extensions::MenuItem::LAUNCHER));
  }

  GURL CreateDefaultManifestURL(const GURL& application_url) {
    return application_url.Resolve("/manifest.webmanifest");
  }

  blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->id = application_url.DeprecatedGetOriginAsURL();
    manifest->scope = application_url.Resolve("/");
    manifest->start_url = application_url.Resolve("/ix.html");
    manifest->display = web_app::DisplayMode::kStandalone;
    manifest->short_name = u"test short manifest name";
    manifest->version = u"1.0.0";

    blink::Manifest::ImageResource icon;
    icon.src = application_url.Resolve(kIconPath);
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    icon.type = u"image/png";
    icon.sizes = {gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize)};
    manifest->icons.push_back(icon);

    return manifest;
  }

  web_app::FakeWebContentsManager& web_contents_manager() {
    return static_cast<web_app::FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

 protected:
  const GURL kDevAppOriginUrl = GURL(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ControlledFrameMenuIconLoaderTest, LoadGetAndRemoveIcon) {
  ControlledFrameMenuIconLoader menu_icon_loader;

  extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", main_rfh()->GetProcess()->GetID(),
      main_rfh()->GetRoutingID(), kTestWebViewInstanceId);

  base::test::TestFuture<void> future;
  menu_icon_loader.SetNotifyOnLoadedCallbackForTesting(
      future.GetRepeatingCallback());
  menu_icon_loader.LoadIcon(browser_context(), /*extension=*/nullptr,
                            extension_key);
  EXPECT_EQ(1u, menu_icon_loader.pending_icons_.size());
  EXPECT_EQ(0u, menu_icon_loader.icons_.size());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(0u, menu_icon_loader.pending_icons_.size());
  EXPECT_EQ(1u, menu_icon_loader.icons_.size());

  gfx::Image icon = menu_icon_loader.GetIcon(extension_key);
  EXPECT_EQ(gfx::kFaviconSize, icon.Height());
  EXPECT_EQ(gfx::kFaviconSize, icon.Width());

  menu_icon_loader.RemoveIcon(extension_key);
  EXPECT_EQ(0u, menu_icon_loader.pending_icons_.size());
  EXPECT_EQ(0u, menu_icon_loader.icons_.size());
}

TEST_F(ControlledFrameMenuIconLoaderTest, MenuManager) {
  extensions::MenuManager* menu_manager = CreateMenuManager();

  // Check that adding a context item starts the icon loading and that the icon
  // is able to be accessed through GetIcon. Also check that the icon is removed
  // when the context item is removed.
  std::unique_ptr<extensions::MenuItem> item = CreateTestItem(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      /*webview_instance_id=*/kTestWebViewInstanceId, /*string_id=*/"test",
      /*visible=*/true);
  const extensions::MenuItem::Id& item_id = item->id();
  auto menu_icon_loader = std::make_unique<ControlledFrameMenuIconLoader>();
  ControlledFrameMenuIconLoader* menu_icon_loader_ptr = menu_icon_loader.get();

  menu_manager->SetMenuIconLoader(item_id.extension_key,
                                  std::move(menu_icon_loader));
  base::test::TestFuture<void> future;
  menu_icon_loader_ptr->SetNotifyOnLoadedCallbackForTesting(
      future.GetRepeatingCallback());
  menu_manager->AddContextItem(/*extension=*/nullptr, std::move(item));
  ASSERT_TRUE(future.Wait());

  // Ensure that grabbing the icon through the MenuManager returns the
  // expected icon.
  EXPECT_EQ(1u, menu_icon_loader_ptr->icons_.size());
  gfx::Image loader_icon = menu_icon_loader_ptr->GetIcon(item_id.extension_key);
  gfx::Image menu_manager_icon =
      menu_manager->GetIconForExtensionKey(item_id.extension_key);
  EXPECT_EQ(loader_icon, menu_manager_icon);

  menu_manager->RemoveContextMenuItem(item_id);
  EXPECT_EQ(0u, menu_icon_loader_ptr->pending_icons_.size());
  EXPECT_EQ(0u, menu_icon_loader_ptr->icons_.size());
}

TEST_F(ControlledFrameMenuIconLoaderTest, ContextMenuMatcher) {
  extensions::MenuManager* menu_manager = CreateMenuManager();

  std::unique_ptr<extensions::MenuItem> item = CreateTestItem(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      /*webview_instance_id=*/kTestWebViewInstanceId, /*string_id=*/"test",
      /*visible=*/true);
  const extensions::MenuItem::Id& item_id = item->id();
  auto menu_icon_loader = std::make_unique<ControlledFrameMenuIconLoader>();
  ControlledFrameMenuIconLoader* menu_icon_loader_ptr = menu_icon_loader.get();

  menu_manager->SetMenuIconLoader(item_id.extension_key,
                                  std::move(menu_icon_loader));
  base::test::TestFuture<void> future;
  menu_icon_loader_ptr->SetNotifyOnLoadedCallbackForTesting(
      future.GetRepeatingCallback());
  menu_manager->AddContextItem(/*extension=*/nullptr, std::move(item));
  ASSERT_TRUE(future.Wait());

  ui::SimpleMenuModel menu_model(/*delegate=*/nullptr);
  auto extension_items = std::make_unique<extensions::ContextMenuMatcher>(
      profile(),
      /*delegate=*/nullptr, &menu_model,
      base::BindLambdaForTesting(
          [](const extensions::MenuItem* item) { return true; }));

  int index = 0;
  extension_items->AppendExtensionItems(item_id.extension_key, std::u16string(),
                                        &index,
                                        /*is_action_menu=*/false);
  gfx::Image icon = menu_model.GetIconAt(/*index=*/0).GetImage();
  EXPECT_EQ(gfx::kFaviconSize, icon.Height());
  EXPECT_EQ(gfx::kFaviconSize, icon.Width());

  web_app::FakeWebContentsManager::FakeIconState& icon_state =
      web_contents_manager().GetOrCreateIconState(
          kDevAppOriginUrl.Resolve(kIconPath));
  ASSERT_EQ(1u, icon_state.bitmaps.size());
  EXPECT_TRUE(cc::MatchesBitmap(icon_state.bitmaps[0], icon.AsBitmap(),
                                cc::ExactPixelComparator()));
}

}  // namespace controlled_frame
