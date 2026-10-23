// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobars_switches.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/test/mock_base_window.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace infobars {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class FakeBrowserWindow {
 public:
  explicit FakeBrowserWindow(TestingProfile* profile,
                             BrowserWindowInterface::Type type =
                                 BrowserWindowInterface::Type::TYPE_NORMAL)
      : profile_(profile), tab_strip_model_(&delegate_, profile) {
    delegate_.SetBrowserWindowInterface(&browser_window_);

    ON_CALL(browser_window_, GetWindow()).WillByDefault(Return(&base_window_));
    ON_CALL(std::as_const(browser_window_), GetWindow())
        .WillByDefault(Return(&base_window_));
    ON_CALL(browser_window_, GetTabStripModel())
        .WillByDefault(Return(&tab_strip_model_));
    ON_CALL(std::as_const(browser_window_), GetTabStripModel())
        .WillByDefault(Return(&tab_strip_model_));
    ON_CALL(browser_window_, GetProfile()).WillByDefault(Return(profile_));
    ON_CALL(std::as_const(browser_window_), GetProfile())
        .WillByDefault(Return(profile_));
    ON_CALL(browser_window_, GetType()).WillByDefault(Return(type));
    ON_CALL(browser_window_, IsDeleteScheduled()).WillByDefault(Return(false));
    ON_CALL(browser_window_, GetActiveTabInterface()).WillByDefault([this]() {
      return tab_strip_model_.GetActiveTab();
    });
    ON_CALL(browser_window_, RegisterActiveTabDidChange(_))
        .WillByDefault(
            [this](BrowserWindowInterface::ActiveTabChangeCallback callback) {
              return active_tab_changed_callbacks_.Add(std::move(callback));
            });

    BrowserCollectionObserver* collection_delegate =
        GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
    collection_delegate->OnBrowserCreated(&browser_window_);
    collection_delegate->OnBrowserActivated(&browser_window_);
  }

  ~FakeBrowserWindow() {
    Close();
    delegate_.SetBrowserWindowInterface(nullptr);
  }

  content::WebContents* AddTab(bool foreground = true,
                               bool render_frame_live = true) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    content::WebContents* raw_contents = web_contents.get();
    ContentInfoBarManager::CreateForWebContents(raw_contents);
    if (render_frame_live) {
      content::RenderFrameHostTester::For(raw_contents->GetPrimaryMainFrame())
          ->InitializeRenderFrameIfNeeded();
    }
    const content::WebContents* old_active =
        tab_strip_model_.GetActiveWebContents();
    tab_strip_model_.AppendWebContents(std::move(web_contents), foreground);
    if (tab_strip_model_.GetActiveWebContents() != old_active) {
      active_tab_changed_callbacks_.Notify(&browser_window_);
    }
    return raw_contents;
  }

  void ActivateTabAt(int index) {
    const content::WebContents* old_active =
        tab_strip_model_.GetActiveWebContents();
    tab_strip_model_.ActivateTabAt(index);
    if (tab_strip_model_.GetActiveWebContents() != old_active) {
      active_tab_changed_callbacks_.Notify(&browser_window_);
    }
  }

  void Close(bool remove_infobars_before_closing_tabs = true) {
    if (closed_) {
      return;
    }
    closed_ = true;
    ON_CALL(browser_window_, IsDeleteScheduled()).WillByDefault(Return(true));
    if (remove_infobars_before_closing_tabs) {
      for (int i = 0; i < tab_strip_model_.count(); ++i) {
        if (auto* mgr = ContentInfoBarManager::FromWebContents(
                tab_strip_model_.GetWebContentsAt(i))) {
          mgr->RemoveAllInfoBars(/*animate=*/false);
        }
      }
    }
    tab_strip_model_.CloseAllTabs();
    active_tab_changed_callbacks_.Notify(&browser_window_);
    BrowserCollectionObserver* collection_delegate =
        GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
    collection_delegate->OnBrowserClosed(&browser_window_);
  }

  void NotifyActiveTabChanged() {
    active_tab_changed_callbacks_.Notify(&browser_window_);
  }

  void DetachDelegateBrowserWindow() {
    delegate_.SetBrowserWindowInterface(nullptr);
  }

  NiceMock<MockBrowserWindowInterface>& window() { return browser_window_; }
  TabStripModel& tab_strip_model() { return tab_strip_model_; }

 private:
  raw_ptr<TestingProfile> profile_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_;
  NiceMock<ui::MockBaseWindow> base_window_;
  NiceMock<MockBrowserWindowInterface> browser_window_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      active_tab_changed_callbacks_;
  bool closed_ = false;
};

class DummyConfirmDelegate : public ConfirmInfoBarDelegate {
 public:
  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
  std::u16string GetMessageText() const override { return u"dummy"; }
};

class ReplaceOnDestroyObserver : public InfoBarManager::Observer {
 public:
  ReplaceOnDestroyObserver(BrowserInfoBarManager* bim,
                           InfoBarManager* observed_manager,
                           InfoBarDelegate::InfoBarIdentifier replacement_id)
      : bim_(bim), replacement_id_(replacement_id) {
    observation_.Observe(observed_manager);
  }

  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override {
    EXPECT_NE(nullptr, infobar->owner());
    replace_result_ = bim_->Replace(infobar, replacement_id_);
    called_ = true;
  }

  void OnManagerWillBeDestroyed(InfoBarManager* manager) override {
    observation_.Reset();
  }

  bool called() const { return called_; }
  InfoBar* replace_result() const { return replace_result_; }

 private:
  raw_ptr<BrowserInfoBarManager> bim_;
  InfoBarDelegate::InfoBarIdentifier replacement_id_;
  base::ScopedObservation<InfoBarManager, InfoBarManager::Observer>
      observation_{this};
  raw_ptr<InfoBar> replace_result_ = nullptr;
  bool called_ = false;
};

class DetachedWebContentsActionObserver : public InfoBarManager::Observer {
 public:
  DetachedWebContentsActionObserver(BrowserInfoBarManager* bim,
                                    InfoBarManager* observed_manager,
                                    InfoBarDelegate::InfoBarIdentifier id)
      : bim_(bim), id_(id) {
    observation_.Observe(observed_manager);
  }

  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override {
    bim_->Hide(id_);
    infobar->delegate()->AsConfirmInfoBarDelegate()->Accept();
  }

  void OnManagerWillBeDestroyed(InfoBarManager* manager) override {
    observation_.Reset();
  }

 private:
  raw_ptr<BrowserInfoBarManager> bim_;
  InfoBarDelegate::InfoBarIdentifier id_;
  base::ScopedObservation<InfoBarManager, InfoBarManager::Observer>
      observation_{this};
};

}  // namespace

class BrowserInfoBarManagerUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NE(nullptr, GlobalBrowserCollection::GetInstance());
    profile_ = std::make_unique<TestingProfile>();

    if (!BrowserInfoBarManager::From(TestingBrowserProcess::GetGlobal())) {
      owned_manager_ = std::make_unique<BrowserInfoBarManager>(
          TestingBrowserProcess::GetGlobal());
    }
  }

  void TearDown() override {
    owned_manager_.reset();
    profile_.reset();
  }

  BrowserInfoBarManager* manager() {
    return BrowserInfoBarManager::From(TestingBrowserProcess::GetGlobal());
  }

  std::unique_ptr<FakeBrowserWindow> CreateBrowserWindow(
      BrowserWindowInterface::Type type =
          BrowserWindowInterface::Type::TYPE_NORMAL) {
    return std::make_unique<FakeBrowserWindow>(profile_.get(), type);
  }

  content::BrowserTaskEnvironment task_environment_;
  ChromeLayoutProvider layout_provider_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BrowserInfoBarManager> owned_manager_;
};

TEST_F(BrowserInfoBarManagerUnitTest, RegisterAndIsRegistered) {
  ASSERT_NE(nullptr, manager());
  const auto id = InfoBarDelegate::TEST_INFOBAR;
  EXPECT_FALSE(manager()->IsRegistered(id));

  manager()->Register(
      InfoBarSpec::Builder(id).SetMessageText(u"Hello").Build());
  EXPECT_TRUE(manager()->IsRegistered(id));
}

TEST_F(BrowserInfoBarManagerUnitTest, ConstructorObservesPreExistingBrowsers) {
  owned_manager_.reset();
  ASSERT_EQ(nullptr,
            BrowserInfoBarManager::From(TestingBrowserProcess::GetGlobal()));

  auto browser = CreateBrowserWindow();
  browser->AddTab();

  owned_manager_ = std::make_unique<BrowserInfoBarManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_EQ(owned_manager_.get(), manager());

  const auto id = InfoBarDelegate::TEST_INFOBAR;
  manager()->Register(InfoBarSpec::Builder(id)
                          .SetMessageText(u"Global message")
                          .SetScope(InfoBarScope::kGlobal)
                          .Build());

  EXPECT_TRUE(manager()->ShowGlobally(id));
  content::WebContents* tab0 = browser->tab_strip_model().GetWebContentsAt(0);
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  content::WebContents* tab1 = browser->AddTab(/*foreground=*/true);
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab1)->infobars().size());
}

TEST_F(BrowserInfoBarManagerUnitTest, ShowEdgeCases) {
  const auto id = InfoBarDelegate::TEST_INFOBAR;
  const auto unregistered_id = InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE;

  auto browser = CreateBrowserWindow();
  browser->AddTab();
  tabs::TabInterface* tab = browser->tab_strip_model().GetActiveTab();

  EXPECT_EQ(nullptr, manager()->Show(tab, unregistered_id));

  manager()->Register(
      InfoBarSpec::Builder(id).SetMessageText(u"Buttonless").Build());

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(nullptr));
  EXPECT_EQ(nullptr, manager()->Show(&mock_tab, id));

  // WebContents without an attached ContentInfoBarManager.
  std::unique_ptr<content::WebContents> bare_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  ON_CALL(mock_tab, GetContents()).WillByDefault(Return(bare_contents.get()));
  EXPECT_EQ(nullptr, manager()->Show(&mock_tab, id));

  infobars::InfoBar* infobar = manager()->Show(tab, id);
  ASSERT_NE(nullptr, infobar);
  EXPECT_EQ(nullptr, manager()->Show(tab, id));

  // With --disable-infobars and --headless, buttonless infobars are rejected
  // by InfoBarManager::ShouldShowInfoBar().
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableInfoBars);
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kHeadless);

  browser->AddTab();
  tabs::TabInterface* disabled_tab = browser->tab_strip_model().GetActiveTab();
  EXPECT_EQ(nullptr, manager()->Show(disabled_tab, id));
}

TEST_F(BrowserInfoBarManagerUnitTest, ReplaceEdgeCases) {
  const auto id_with_button = InfoBarDelegate::TEST_INFOBAR;
  const auto id_buttonless = InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE;
  const auto unregistered_id = InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE;

  manager()->Register(InfoBarSpec::Builder(id_with_button)
                          .SetMessageText(u"With button")
                          .AddOkButton(u"OK", base::DoNothing())
                          .SetAllowDuplicates(true)
                          .Build());
  manager()->Register(InfoBarSpec::Builder(id_buttonless)
                          .SetMessageText(u"Buttonless")
                          .SetAllowDuplicates(true)
                          .Build());

  auto browser = CreateBrowserWindow();
  content::WebContents* contents = browser->AddTab();
  tabs::TabInterface* tab = browser->tab_strip_model().GetActiveTab();

  infobars::InfoBar* infobar = manager()->Show(tab, id_with_button);
  ASSERT_NE(nullptr, infobar);
  EXPECT_EQ(nullptr, manager()->Replace(infobar, unregistered_id));

  infobars::InfoBar* replaced = manager()->Replace(infobar, id_buttonless);
  ASSERT_NE(nullptr, replaced);
  EXPECT_EQ(u"Buttonless",
            replaced->delegate()->AsConfirmInfoBarDelegate()->GetMessageText());

  std::unique_ptr<infobars::InfoBar> unowned_infobar =
      CreateConfirmInfoBar(std::make_unique<DummyConfirmDelegate>());
  ASSERT_EQ(nullptr, unowned_infobar->owner());
  EXPECT_EQ(nullptr, manager()->Replace(unowned_infobar.get(), id_buttonless));

  // During ~WebContents(), WebContentsDestroyed() clears the delegate's
  // web_contents() before ~ContentInfoBarManager() removes remaining infobars,
  // while owner() is still non-null.
  auto* content_manager = ContentInfoBarManager::FromWebContents(contents);
  ReplaceOnDestroyObserver observer(manager(), content_manager, id_buttonless);
  browser->tab_strip_model().CloseAllTabs();
  EXPECT_TRUE(observer.called());
  EXPECT_EQ(nullptr, observer.replace_result());

  // With --disable-infobars and --headless, replacing an allowed infobar (with
  // buttons) with a buttonless infobar is rejected by ReplaceInfoBar().
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableInfoBars);
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kHeadless);

  browser->AddTab();
  tabs::TabInterface* disabled_tab = browser->tab_strip_model().GetActiveTab();
  infobars::InfoBar* infobar_with_btn =
      manager()->Show(disabled_tab, id_with_button);
  ASSERT_NE(nullptr, infobar_with_btn);
  EXPECT_EQ(nullptr, manager()->Replace(infobar_with_btn, id_buttonless));
}

TEST_F(BrowserInfoBarManagerUnitTest, HideOverloadsAndTrackedGlobalInstances) {
  const auto tab_id = InfoBarDelegate::TEST_INFOBAR;
  const auto other_tab_id = InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE;
  const auto global_id = InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE;
  const auto unregistered_id = InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE;

  manager()->Register(
      InfoBarSpec::Builder(tab_id).SetMessageText(u"Tab 1").Build());
  manager()->Register(
      InfoBarSpec::Builder(other_tab_id).SetMessageText(u"Tab 2").Build());
  manager()->Register(InfoBarSpec::Builder(global_id)
                          .SetMessageText(u"Global")
                          .SetScope(InfoBarScope::kGlobal)
                          .Build());

  std::unique_ptr<infobars::InfoBar> unowned_infobar =
      CreateConfirmInfoBar(std::make_unique<DummyConfirmDelegate>());
  manager()->Hide(unowned_infobar.get());

  manager()->Hide(unregistered_id);
  manager()->Hide(global_id);

  auto browser = CreateBrowserWindow();

  std::unique_ptr<content::WebContents> bare_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  manager()->Hide(bare_contents.get(), unregistered_id);
  manager()->Hide(bare_contents.get(), tab_id);

  content::WebContents* tab0 = browser->AddTab(/*foreground=*/true);
  content::WebContents* tab1 = browser->AddTab(/*foreground=*/false);
  tabs::TabInterface* tab0_interface =
      browser->tab_strip_model().GetTabAtIndex(0);
  tabs::TabInterface* tab1_interface =
      browser->tab_strip_model().GetTabAtIndex(1);

  infobars::InfoBar* tab0_infobar = manager()->Show(tab0_interface, tab_id);
  ASSERT_NE(nullptr, tab0_infobar);
  ASSERT_NE(nullptr, manager()->Show(tab0_interface, other_tab_id));
  EXPECT_EQ(2u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());
  manager()->Hide(tab0, other_tab_id);
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  manager()->Hide(tab0_infobar);
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  ASSERT_NE(nullptr, manager()->Show(tab0_interface, tab_id));
  manager()->Hide(tab0, tab_id);

  // Tracked global instances are ignored by the WebContents-scoped overload.
  manager()->Hide(tab0, global_id);
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  // A tab-scoped override of a global identifier on another tab is removed.
  InfoBarShowParams tab_override_params;
  tab_override_params.scope = InfoBarScope::kTab;
  ASSERT_NE(nullptr, manager()->Show(tab1_interface, global_id,
                                     std::move(tab_override_params)));
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab1)->infobars().size());
  manager()->Hide(tab1, global_id);
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(tab1)->infobars().size());
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  manager()->Hide(global_id);
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());
}

TEST_F(BrowserInfoBarManagerUnitTest, DelegatePropertiesAndButtonGetters) {
  const auto id_custom = InfoBarDelegate::TEST_INFOBAR;
  const auto id_defaults = InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE;

  bool custom_view_created = false;
  manager()->Register(
      InfoBarSpec::Builder(id_custom)
          .SetMessageTextTemplate(u"Hello $1")
          .SetLinkText(u"Spec link")
          .SetLinkAccessibleText(u"Accessible link")
          .SetLinkNavigationUrl(GURL("https://example.com/help"))
          .SetShouldShowLinkBeforeButton(true)
          .SetLinkSpacingWhenPositionedBeforeButton(14)
          .SetIconId(42)
          .SetIcon(vector_icons::kInfoOutlineOldIcon)
          .SetDarkModeIcon(vector_icons::kWarningIcon)
          .AddOkButton(u"Spec OK", base::DoNothing())
          .AddCancelButton(u"Spec Cancel", base::DoNothing())
          .AddExtraButton(u"Spec Extra", base::DoNothing())
          .SetExpireOnNavigation(true)
          .SetShouldHideInFullscreen(true)
          .SetShouldAnimate(false)
          .SetIsCloseable(false)
          .SetPriority(InfoBarDelegate::InfobarPriority::kCriticalSecurity)
          .SetAllowDuplicates(true)
          .SetCustomViewCallback(base::BindLambdaForTesting(
              [&](content::WebContents*) -> std::unique_ptr<views::View> {
                custom_view_created = true;
                return std::make_unique<views::Label>(u"Custom spec view");
              }))
          .Build());

  manager()->Register(InfoBarSpec::Builder(id_defaults)
                          .SetMessageTextTemplate(u"Template without callback")
                          .SetExpireOnNavigation(false)
                          .SetAllowDuplicates(true)
                          .Build());

  auto browser = CreateBrowserWindow();
  browser->AddTab();
  tabs::TabInterface* tab = browser->tab_strip_model().GetActiveTab();

  InfoBarShowParams params;
  params.ok_button.label = u"Param OK";
  params.ok_button.image =
      ui::ImageModel::FromVectorIcon(vector_icons::kInfoOutlineOldIcon);
  params.ok_button.enabled = false;
  params.ok_button.tooltip = u"OK Tooltip";
  params.ok_button.style = ui::ButtonStyle::kProminent;
  params.ok_button.use_text_color_for_icon = false;
  params.substitutions = {
      MessageSubstitution(u"World", /*is_link=*/false, std::nullopt)};

  infobars::InfoBar* infobar =
      manager()->Show(tab, id_custom, std::move(params));
  ASSERT_NE(nullptr, infobar);
  EXPECT_TRUE(custom_view_created);

  auto* delegate = infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_NE(nullptr, delegate);

  EXPECT_EQ(id_custom, delegate->GetIdentifier());
  EXPECT_FALSE(delegate->EqualsDelegate(delegate));
  EXPECT_EQ(u"Hello World", delegate->GetMessageText());
  EXPECT_EQ(u"Hello $1", delegate->GetMessageTextTemplate());
  EXPECT_EQ(1u, delegate->GetMessageSubstitutions().size());
  EXPECT_EQ(u"Spec link", delegate->GetLinkText());
  EXPECT_EQ(std::optional<std::u16string>(u"Accessible link"),
            delegate->GetLinkAccessibleText());
  EXPECT_EQ(GURL("https://example.com/help"), delegate->GetLinkURL());
  EXPECT_TRUE(delegate->ShouldShowLinkBeforeButton());
  EXPECT_EQ(14, delegate->GetLinkSpacingWhenPositionedBeforeButton());
  EXPECT_EQ(42, delegate->GetIconId());

  delegate->set_dark_mode(false);
  EXPECT_EQ(&vector_icons::kInfoOutlineOldIcon, &delegate->GetVectorIcon());
  delegate->set_dark_mode(true);
  EXPECT_EQ(&vector_icons::kWarningIcon, &delegate->GetVectorIcon());

  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK |
                ConfirmInfoBarDelegate::BUTTON_CANCEL |
                ConfirmInfoBarDelegate::BUTTON_EXTRA,
            delegate->GetButtons());
  EXPECT_EQ(u"Param OK",
            delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  EXPECT_EQ(u"Spec Cancel",
            delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  EXPECT_EQ(u"Spec Extra",
            delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_EXTRA));

  EXPECT_FALSE(
      delegate->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_OK).IsEmpty());
  EXPECT_TRUE(delegate->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_CANCEL)
                  .IsEmpty());
  EXPECT_TRUE(
      delegate->GetButtonImage(ConfirmInfoBarDelegate::BUTTON_NONE).IsEmpty());

  EXPECT_FALSE(delegate->GetButtonEnabled(ConfirmInfoBarDelegate::BUTTON_OK));
  EXPECT_TRUE(
      delegate->GetButtonEnabled(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  EXPECT_TRUE(delegate->GetButtonEnabled(ConfirmInfoBarDelegate::BUTTON_NONE));

  EXPECT_EQ(u"OK Tooltip",
            delegate->GetButtonTooltip(ConfirmInfoBarDelegate::BUTTON_OK));
  EXPECT_EQ(u"",
            delegate->GetButtonTooltip(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  EXPECT_EQ(u"",
            delegate->GetButtonTooltip(ConfirmInfoBarDelegate::BUTTON_NONE));

  EXPECT_EQ(ui::ButtonStyle::kProminent,
            delegate->GetButtonStyle(ConfirmInfoBarDelegate::BUTTON_OK));
  EXPECT_EQ(std::nullopt,
            delegate->GetButtonStyle(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  EXPECT_EQ(std::nullopt,
            delegate->GetButtonStyle(ConfirmInfoBarDelegate::BUTTON_NONE));

  EXPECT_FALSE(delegate->ShouldUseTextColorForButtonIcon(
      ConfirmInfoBarDelegate::BUTTON_OK));
  EXPECT_TRUE(delegate->ShouldUseTextColorForButtonIcon(
      ConfirmInfoBarDelegate::BUTTON_CANCEL));
  EXPECT_TRUE(delegate->ShouldUseTextColorForButtonIcon(
      ConfirmInfoBarDelegate::BUTTON_NONE));

  EXPECT_TRUE(delegate->ShouldHideInFullscreen());
  EXPECT_FALSE(delegate->ShouldAnimate());
  EXPECT_FALSE(delegate->IsCloseable());
  EXPECT_EQ(InfoBarDelegate::InfobarPriority::kCriticalSecurity,
            delegate->GetPriority());

  InfoBarDelegate::NavigationDetails nav_details;
  nav_details.entry_id = 999;
  nav_details.is_navigation_to_different_page = true;
  nav_details.did_replace_entry = false;
  nav_details.is_reload = false;
  nav_details.is_redirect = false;
  EXPECT_TRUE(delegate->ShouldExpire(nav_details));

  bool param_custom_view_called = false;
  InfoBarShowParams override_params;
  override_params.message_text = u"Overridden message";
  override_params.link_text = u"Overridden link";
  override_params.cancel_button.label = u"Param Cancel";
  override_params.extra_button.label = u"Param Extra";
  override_params.custom_view_callback = base::BindLambdaForTesting(
      [&](content::WebContents*) -> std::unique_ptr<views::View> {
        param_custom_view_called = true;
        return std::make_unique<views::Label>(u"Param view");
      });

  infobars::InfoBar* override_infobar =
      manager()->Show(tab, id_custom, std::move(override_params));
  ASSERT_NE(nullptr, override_infobar);
  EXPECT_TRUE(param_custom_view_called);
  auto* override_delegate =
      override_infobar->delegate()->AsConfirmInfoBarDelegate();
  EXPECT_EQ(u"Overridden message", override_delegate->GetMessageText());
  EXPECT_EQ(u"", override_delegate->GetMessageTextTemplate());
  EXPECT_EQ(u"Overridden link", override_delegate->GetLinkText());
  EXPECT_EQ(u"Param Cancel", override_delegate->GetButtonLabel(
                                 ConfirmInfoBarDelegate::BUTTON_CANCEL));
  EXPECT_EQ(u"Param Extra", override_delegate->GetButtonLabel(
                                ConfirmInfoBarDelegate::BUTTON_EXTRA));

  infobars::InfoBar* defaults_infobar = manager()->Show(tab, id_defaults);
  ASSERT_NE(nullptr, defaults_infobar);
  auto* defaults_delegate =
      defaults_infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_NE(nullptr, defaults_delegate);
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_NONE,
            defaults_delegate->GetButtons());
  EXPECT_TRUE(defaults_delegate->GetMessageSubstitutions().empty());
  EXPECT_TRUE(defaults_delegate->GetVectorIcon().is_empty());
  EXPECT_FALSE(
      defaults_delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)
          .empty());
  EXPECT_FALSE(
      defaults_delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL)
          .empty());
  EXPECT_TRUE(
      defaults_delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_EXTRA)
          .empty());
  EXPECT_FALSE(defaults_delegate->ShouldExpire(nav_details));

  InfoBarShowParams callback_only_params;
  callback_only_params.ok_button_callback = base::DoNothing();
  callback_only_params.cancel_button_callback = base::DoNothing();
  callback_only_params.extra_button_callback = base::DoNothing();
  infobars::InfoBar* callback_only_infobar =
      manager()->Show(tab, id_defaults, std::move(callback_only_params));
  ASSERT_NE(nullptr, callback_only_infobar);
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK |
                ConfirmInfoBarDelegate::BUTTON_CANCEL |
                ConfirmInfoBarDelegate::BUTTON_EXTRA,
            callback_only_infobar->delegate()
                ->AsConfirmInfoBarDelegate()
                ->GetButtons());
}

TEST_F(BrowserInfoBarManagerUnitTest,
       ActionCallbackSynchronouslyDestroyingDelegateReportsOnce) {
  const auto id = InfoBarDelegate::TEST_INFOBAR;
  std::vector<InfoBarResult> results;
  bool callback_ran = false;

  manager()->Register(
      InfoBarSpec::Builder(id)
          .SetMessageText(u"Destroy in callback")
          .AddOkButton(
              u"OK",
              base::BindLambdaForTesting([&](content::WebContents* contents) {
                callback_ran = true;
                ContentInfoBarManager::FromWebContents(contents)
                    ->RemoveAllInfoBars(/*animate=*/false);
              }))
          .SetResultCallback(base::BindLambdaForTesting(
              [&](content::WebContents*, InfoBarResult result) {
                results.push_back(result);
              }))
          .Build());

  auto browser = CreateBrowserWindow();
  browser->AddTab();
  tabs::TabInterface* tab = browser->tab_strip_model().GetActiveTab();

  infobars::InfoBar* infobar = manager()->Show(tab, id);
  ASSERT_NE(nullptr, infobar);
  auto* delegate = infobar->delegate()->AsConfirmInfoBarDelegate();

  EXPECT_FALSE(delegate->Accept());
  EXPECT_TRUE(callback_ran);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kAccepted, results[0]);
}

TEST_F(BrowserInfoBarManagerUnitTest,
       DelegateInteractionsAndHideDuringInteraction) {
  const auto id = InfoBarDelegate::TEST_INFOBAR;
  std::vector<InfoBarResult> results;

  manager()->Register(
      InfoBarSpec::Builder(id)
          .SetMessageTextTemplate(u"Click $1")
          .SetSubstitutionsCallback(
              base::BindLambdaForTesting([](content::WebContents*) {
                return std::vector<MessageSubstitution>{MessageSubstitution(
                    u"here", /*is_link=*/true, std::nullopt)};
              }))
          .SetLinkText(u"Learn more")
          .SetCloseOnAccept(false)
          .SetCloseOnCancel(false)
          .SetCloseOnExtraButton(false)
          .AddOkButton(u"OK", base::DoNothing())
          .AddCancelButton(u"Cancel", base::DoNothing())
          .AddExtraButton(u"Extra", base::DoNothing())
          .SetDismissAction(base::DoNothing())
          .SetInlineLinkCallback(base::BindLambdaForTesting(
              [](content::WebContents*, size_t, WindowOpenDisposition) {
                return false;
              }))
          .SetResultCallback(base::BindLambdaForTesting(
              [&](content::WebContents*, InfoBarResult result) {
                results.push_back(result);
              }))
          .SetAllowDuplicates(true)
          .Build());

  auto browser = CreateBrowserWindow();
  content::WebContents* contents = browser->AddTab();
  tabs::TabInterface* tab = browser->tab_strip_model().GetActiveTab();

  // Calling Hide(infobar) during a non-closing action forces close on return.
  infobars::InfoBar* infobar = nullptr;
  InfoBarShowParams params;
  params.cancel_button_callback = base::BindLambdaForTesting(
      [&](content::WebContents*) { manager()->Hide(infobar); });
  infobar = manager()->Show(tab, id, std::move(params));
  ASSERT_NE(nullptr, infobar);
  EXPECT_TRUE(infobar->delegate()->AsConfirmInfoBarDelegate()->Cancel());
  infobar->RemoveSelf();
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kCancelled, results[0]);

  // Spec callbacks for ExtraButtonPressed, Cancel, and InfoBarDismissed.
  results.clear();
  infobars::InfoBar* infobar2 = manager()->Show(tab, id);
  ASSERT_NE(nullptr, infobar2);
  EXPECT_FALSE(
      infobar2->delegate()->AsConfirmInfoBarDelegate()->ExtraButtonPressed());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kExtraButtonPressed, results[0]);
  EXPECT_FALSE(infobar2->delegate()->AsConfirmInfoBarDelegate()->Cancel());
  infobar2->delegate()->InfoBarDismissed();
  infobar2->RemoveSelf();
  EXPECT_EQ(1u, results.size());

  // InfoBarShowParams callback overrides for Accept, ExtraButtonPressed, and
  // result_callback.
  bool param_ok_called = false;
  bool param_extra_called = false;
  std::vector<InfoBarResult> param_results;
  InfoBarShowParams action_override_params;
  action_override_params.ok_button_callback = base::BindLambdaForTesting(
      [&](content::WebContents*) { param_ok_called = true; });
  action_override_params.extra_button_callback = base::BindLambdaForTesting(
      [&](content::WebContents*) { param_extra_called = true; });
  action_override_params.result_callback = base::BindLambdaForTesting(
      [&](content::WebContents*, InfoBarResult result) {
        param_results.push_back(result);
      });
  infobars::InfoBar* override_infobar =
      manager()->Show(tab, id, std::move(action_override_params));
  ASSERT_NE(nullptr, override_infobar);
  EXPECT_FALSE(
      override_infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  EXPECT_TRUE(param_ok_called);
  EXPECT_FALSE(override_infobar->delegate()
                   ->AsConfirmInfoBarDelegate()
                   ->ExtraButtonPressed());
  EXPECT_TRUE(param_extra_called);
  override_infobar->RemoveSelf();
  ASSERT_EQ(1u, param_results.size());
  EXPECT_EQ(InfoBarResult::kAccepted, param_results[0]);

  results.clear();
  infobars::InfoBar* infobar3 = manager()->Show(tab, id);
  ASSERT_NE(nullptr, infobar3);
  EXPECT_FALSE(infobar3->delegate()
                   ->AsConfirmInfoBarDelegate()
                   ->InlineSubstitutionLinkClicked(
                       0u, WindowOpenDisposition::kCurrentTab));
  EXPECT_FALSE(infobar3->delegate()->LinkClicked(
      WindowOpenDisposition::kNewBackgroundTab));
  ContentInfoBarManager::FromWebContents(contents)->RemoveInfoBar(infobar3);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kLinkClicked, results[0]);

  results.clear();
  InfoBarShowParams inline_params;
  inline_params.inline_link_callback =
      base::BindLambdaForTesting([](content::WebContents*, size_t,
                                    WindowOpenDisposition) { return true; });
  infobars::InfoBar* infobar4 =
      manager()->Show(tab, id, std::move(inline_params));
  ASSERT_NE(nullptr, infobar4);
  EXPECT_TRUE(infobar4->delegate()
                  ->AsConfirmInfoBarDelegate()
                  ->InlineSubstitutionLinkClicked(
                      0u, WindowOpenDisposition::kCurrentTab));
  infobar4->RemoveSelf();
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kLinkClicked, results[0]);

  // Hiding inside a closing inline-link callback suppresses result reporting.
  results.clear();
  infobars::InfoBar* infobar5 = nullptr;
  InfoBarShowParams inline_hide_params;
  inline_hide_params.inline_link_callback = base::BindLambdaForTesting(
      [&](content::WebContents*, size_t, WindowOpenDisposition) {
        manager()->Hide(infobar5);
        return true;
      });
  infobar5 = manager()->Show(tab, id, std::move(inline_hide_params));
  ASSERT_NE(nullptr, infobar5);
  EXPECT_TRUE(infobar5->delegate()
                  ->AsConfirmInfoBarDelegate()
                  ->InlineSubstitutionLinkClicked(
                      0u, WindowOpenDisposition::kCurrentTab));
  infobar5->RemoveSelf();
  EXPECT_TRUE(results.empty());
}

TEST_F(BrowserInfoBarManagerUnitTest, GlobalShowFilterAndTabSwitching) {
  const auto global_id = InfoBarDelegate::TEST_INFOBAR;
  const auto unregistered_id = InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE;

  EXPECT_FALSE(manager()->ShowGlobally(unregistered_id));

  FakeBrowserWindow* active_browser_ptr = nullptr;
  manager()->Register(
      InfoBarSpec::Builder(global_id)
          .SetMessageText(u"Global message")
          .SetScope(InfoBarScope::kGlobal)
          .SetCloseOnAccept(false)
          .SetBrowserFilter(
              base::BindLambdaForTesting([](BrowserWindowInterface* bwi) {
                return bwi->GetType() ==
                       BrowserWindowInterface::Type::TYPE_NORMAL;
              }))
          .AddOkButton(u"OK",
                       base::BindLambdaForTesting([&](content::WebContents*) {
                         if (active_browser_ptr) {
                           active_browser_ptr->AddTab(/*foreground=*/true);
                           active_browser_ptr->ActivateTabAt(0);
                         }
                       }))
          .Build());

  EXPECT_FALSE(manager()->ShowGlobally(global_id));

  auto popup_browser =
      CreateBrowserWindow(BrowserWindowInterface::Type::TYPE_POPUP);
  popup_browser->AddTab();
  EXPECT_FALSE(manager()->ShowGlobally(global_id));

  auto browser = CreateBrowserWindow();
  content::WebContents* tab0 = browser->AddTab();

  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  EXPECT_FALSE(manager()->ShowGlobally(global_id));

  content::WebContents* popup_tab1 = popup_browser->AddTab(/*foreground=*/true);
  EXPECT_EQ(
      0u,
      ContentInfoBarManager::FromWebContents(popup_tab1)->infobars().size());

  browser->NotifyActiveTabChanged();
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  // Round-trip tab switch mid-interaction keeps the infobar on the origin tab.
  active_browser_ptr = browser.get();
  infobars::InfoBar* tab0_infobar =
      ContentInfoBarManager::FromWebContents(tab0)->infobars()[0];
  EXPECT_FALSE(tab0_infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());

  // Normal tab switch moves the global infobar to the newly activated tab.
  active_browser_ptr = nullptr;
  browser->ActivateTabAt(1);
  content::WebContents* tab1 = browser->tab_strip_model().GetWebContentsAt(1);
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(tab0)->infobars().size());
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(tab1)->infobars().size());
}

TEST_F(BrowserInfoBarManagerUnitTest, GlobalCascadeCloseAndTeardown) {
  const auto global_id = InfoBarDelegate::TEST_INFOBAR;
  std::vector<InfoBarResult> results;

  manager()->Register(InfoBarSpec::Builder(global_id)
                          .SetMessageText(u"Global")
                          .SetScope(InfoBarScope::kGlobal)
                          .AddOkButton(u"OK", base::DoNothing())
                          .SetInlineLinkCallback(base::BindLambdaForTesting(
                              [](content::WebContents*, size_t,
                                 WindowOpenDisposition) { return true; }))
                          .SetResultCallback(base::BindLambdaForTesting(
                              [&](content::WebContents*, InfoBarResult result) {
                                results.push_back(result);
                              }))
                          .Build());

  auto browser1 = CreateBrowserWindow();
  content::WebContents* b1_tab0 = browser1->AddTab();
  auto browser2 = CreateBrowserWindow();
  content::WebContents* b2_tab0 = browser2->AddTab();

  // Removing in one live window cascades across windows and reports kIgnored.
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  ContentInfoBarManager::FromWebContents(b1_tab0)->RemoveInfoBar(
      ContentInfoBarManager::FromWebContents(b1_tab0)->infobars()[0]);
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(b1_tab0)->infobars().size());
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(b2_tab0)->infobars().size());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kIgnored, results[0]);

  // Closing via Accept() hides across windows and reports kAccepted.
  results.clear();
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  infobars::InfoBar* b1_infobar =
      ContentInfoBarManager::FromWebContents(b1_tab0)->infobars()[0];
  EXPECT_TRUE(b1_infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  b1_infobar->RemoveSelf();
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(b1_tab0)->infobars().size());
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(b2_tab0)->infobars().size());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kAccepted, results[0]);

  // Closing via InlineSubstitutionLinkClicked() hides across windows.
  results.clear();
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  infobars::InfoBar* b2_infobar =
      ContentInfoBarManager::FromWebContents(b2_tab0)->infobars()[0];
  EXPECT_TRUE(b2_infobar->delegate()
                  ->AsConfirmInfoBarDelegate()
                  ->InlineSubstitutionLinkClicked(
                      0u, WindowOpenDisposition::kCurrentTab));
  b2_infobar->RemoveSelf();
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(b1_tab0)->infobars().size());
  EXPECT_EQ(0u,
            ContentInfoBarManager::FromWebContents(b2_tab0)->infobars().size());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kLinkClicked, results[0]);

  // Closing one window while another remains active reports only when the last
  // window closes.
  results.clear();
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  browser1->Close();
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(1u,
            ContentInfoBarManager::FromWebContents(b2_tab0)->infobars().size());
  browser2->Close();
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(InfoBarResult::kIgnored, results[0]);
  manager()->Hide(global_id);

  // Removal when TabInterface::GetBrowserWindowInterface() is null, when
  // !IsRenderFrameLive(), when ContentInfoBarManager is destroyed, and when
  // web_contents() is already detached during tab closure.
  auto browser3 = CreateBrowserWindow();
  content::WebContents* b3_tab0 =
      browser3->AddTab(/*foreground=*/true, /*render_frame_live=*/false);
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  ContentInfoBarManager::FromWebContents(b3_tab0)->RemoveAllInfoBars(false);
  manager()->Hide(global_id);

  content::WebContents* b3_tab1 = browser3->AddTab(/*foreground=*/true);
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  browser3->DetachDelegateBrowserWindow();
  ContentInfoBarManager::FromWebContents(b3_tab1)->RemoveAllInfoBars(false);
  // Exercise OnManagerWillBeDestroyed() cleanup for last_active_managers_.
  b3_tab1->RemoveUserData(ContentInfoBarManager::UserDataKey());
  manager()->Hide(global_id);

  auto browser4 = CreateBrowserWindow();
  content::WebContents* b4_tab0 = browser4->AddTab(/*foreground=*/true);
  ASSERT_TRUE(manager()->ShowGlobally(global_id));
  DetachedWebContentsActionObserver detached_observer(
      manager(), ContentInfoBarManager::FromWebContents(b4_tab0), global_id);
  browser4->Close(/*remove_infobars_before_closing_tabs=*/false);
}

}  // namespace infobars
