// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "components/prefs/pref_service.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

namespace {

class TestGlicSelectionObserver : public GlicSelectionObserver {
 public:
  explicit TestGlicSelectionObserver(content::WebContents* web_contents)
      : GlicSelectionObserver(web_contents) {}

  using GlicSelectionObserver::PrimaryMainFrameWasResized;

  void UpdateSelectionState(const std::u16string& text,
                            bool is_pending_selection,
                            SelectionSource source) override {
    last_processed_text_ = text;
    update_count_++;
    if (call_base_update_selection_state_) {
      GlicSelectionObserver::UpdateSelectionState(text, is_pending_selection,
                                                  source);
    }
  }

  void DismissUI(DismissReason reason) override {
    dismiss_ui_called_ = true;
    dismiss_ui_reason_ = reason;
    GlicSelectionObserver::DismissUI(reason);
  }

  const std::optional<std::u16string>& last_processed_text() const {
    return last_processed_text_;
  }

  int update_count() const { return update_count_; }

  bool dismiss_ui_called() const { return dismiss_ui_called_; }
  std::optional<DismissReason> dismiss_ui_reason() const {
    return dismiss_ui_reason_;
  }

  void Reset() {
    last_processed_text_.reset();
    update_count_ = 0;
    dismiss_ui_called_ = false;
    dismiss_ui_reason_ = std::nullopt;
    call_base_update_selection_state_ = false;
    mock_panel_showing_ = false;
    send_context_called_ = false;
    last_sent_context_.reset();
    show_selection_affordance_called_ = false;
    last_affordance_text_.reset();
    trigger_region_capture_called_ = false;
    mock_side_panel_open_ = true;
    show_selection_overlay_called_ = false;
  }

  // Expose methods for testing.
  using GlicSelectionObserver::IsShakeTriggerEnabled;
  using GlicSelectionObserver::IsSidePanelOpen;
  using GlicSelectionObserver::OnInputEvent;
  using GlicSelectionObserver::OnPageContextEligibilityChanged;
  using GlicSelectionObserver::RenderFrameCreated;
  using GlicSelectionObserver::RenderFrameDeleted;
  using GlicSelectionObserver::ShouldShowSelectionWidget;

  void set_call_base_update_selection_state(bool value) {
    call_base_update_selection_state_ = value;
  }

  void set_mock_panel_showing(bool value) { mock_panel_showing_ = value; }
  void set_mock_side_panel_open(bool value) { mock_side_panel_open_ = value; }
  bool BaseIsSidePanelOpen() const {
    return GlicSelectionObserver::IsSidePanelOpen();
  }

  bool send_context_called() const { return send_context_called_; }
  const std::optional<std::u16string>& last_sent_context() const {
    return last_sent_context_;
  }

  bool show_selection_affordance_called() const {
    return show_selection_affordance_called_;
  }
  const std::optional<std::u16string>& last_affordance_text() const {
    return last_affordance_text_;
  }

  bool trigger_region_capture_called() const {
    return trigger_region_capture_called_;
  }

  bool show_selection_overlay_called() const {
    return show_selection_overlay_called_;
  }

 protected:
  bool IsSelectionPromptEnabled() const override { return true; }

  bool IsPanelShowing(tabs::TabInterface* tab_interface,
                      BrowserWindowInterface* bwi) override {
    return mock_panel_showing_;
  }

  void SendAdditionalContextToPanel(
      tabs::TabInterface* tab_interface,
      const std::u16string& selected_text) override {
    if (!IsPageContextEligible() && !selected_text.empty()) {
      return;
    }
    last_sent_context_ = selected_text;
    send_context_called_ = true;
  }

  void ShowSelectionAffordance(const std::u16string& selected_text,
                               BrowserWindowInterface* bwi) override {
    show_selection_affordance_called_ = true;
    last_affordance_text_ = selected_text;
  }

  void TriggerRegionCapture() override {
    trigger_region_capture_called_ = true;
    GlicSelectionObserver::TriggerRegionCapture();
  }

  bool IsSidePanelOpen() const override { return mock_side_panel_open_; }

  void ShowSelectionOverlay() override {
    show_selection_overlay_called_ = true;
    GlicSelectionObserver::ShowSelectionOverlay();
  }

 private:
  std::optional<std::u16string> last_processed_text_;
  int update_count_ = 0;
  bool dismiss_ui_called_ = false;
  std::optional<DismissReason> dismiss_ui_reason_;

  bool call_base_update_selection_state_ = false;
  bool mock_panel_showing_ = false;
  bool mock_side_panel_open_ = true;
  bool send_context_called_ = false;
  std::optional<std::u16string> last_sent_context_;
  bool show_selection_affordance_called_ = false;
  std::optional<std::u16string> last_affordance_text_;
  bool trigger_region_capture_called_ = false;
  bool show_selection_overlay_called_ = false;
};

}  // namespace

namespace {
bool g_mock_eligibility = true;
bool MockIsEligibleWithAccount(
    const std::string&,
    const std::string&,
    const std::string&,
    const std::vector<optimization_guide::FrameMetadata>&) {
  return g_mock_eligibility;
}
optimization_guide::StringViewSpan MockGetMeta(
    std::string_view,
    std::string_view,
    const std::vector<optimization_guide::FrameMetadata>&) {
  return optimization_guide::StringViewSpan{.data = nullptr, .size = 0};
}
optimization_guide::PageEligibilityResult MockCheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>&) {
  return optimization_guide::PageEligibilityResult{
      .status = optimization_guide::PageEligibility::kEligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}
optimization_guide::PageContextEligibilityAPI g_test_api = {
    .IsPageContextEligible = nullptr,
    .IsPageContextEligibleWithAccount = &MockIsEligibleWithAccount,
    .ShouldReextractPageContext = nullptr,
    .GetMetaTagNamesAffectingEligibility = &MockGetMeta,
    .CheckPageEligibility = &MockCheckPageEligibility,
};
}  // namespace

class GlicSelectionObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicSelectionObserverTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicSelectionPrompt);
    ChromeRenderViewHostTestHarness::SetUp();

    test_eligibility_holder_ =
        std::make_unique<optimization_guide::PageContextEligibility>(
            &g_test_api);
    optimization_guide::PageContextEligibility::SetForTesting(
        test_eligibility_holder_.get());

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

    // Create our test observer.
    observer_ = std::make_unique<TestGlicSelectionObserver>(web_contents());
    task_environment()->RunUntilIdle();
  }

  void TearDown() override {
    observer_.reset();
    identity_test_env_adaptor_.reset();
    optimization_guide::PageContextEligibility::SetForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  void SetMockEligibility(bool eligible) {
    g_mock_eligibility = eligible;
    NavigateAndCommit(GURL("https://example.com/"));
  }

  // To simulate creating the observer AFTER mock identity state is set.
  void RecreateObserver() {
    observer_ = std::make_unique<TestGlicSelectionObserver>(web_contents());
    task_environment()->RunUntilIdle();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestGlicSelectionObserver> observer_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<optimization_guide::PageContextEligibility>
      test_eligibility_holder_;

  TestGlicSelectionObserver* GetObserver() { return observer_.get(); }

  bool ShouldShowSelectionWidget() {
    return observer_->ShouldShowSelectionWidget();
  }

  void CallOnHide() { observer_->OnHide(); }
  void CallOnAskGemini() { observer_->OnAskGemini(); }

  void CallOnLinkGenerated(
      const GURL& fallback_url,
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status) {
    observer_->OnLinkGenerated(fallback_url, selector, error, ready_status);
  }

  void CallCopyLinkToHighlight(content::WeakDocumentPtr weak_document_ptr) {
    observer_->CopyLinkToHighlight(weak_document_ptr);
  }

  void InvokeGlicFromSelectionAffordance(
      std::u16string selected_text,
      bool is_widget,
      base::WeakPtr<content::WebContents> web_contents,
      std::u16string prompt_override = u"",
      const GlicSkillOption& skill = {},
      const std::string& skill_prompt = "") {
    GlicSelectionObserver::InvokeGlicFromSelectionAffordance(
        selected_text, is_widget, web_contents, prompt_override, skill,
        skill_prompt);
  }

  std::optional<GURL> GetGeneratedLink() const {
    return observer_->generated_link_;
  }

  content::RenderWidgetHost* GetRenderWidgetHost() {
    return web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  }

  size_t GetObservedFramesCount() const {
    return observer_->observed_frames_.size();
  }

  void SimulateMouseMove(float x, float y) {
    blink::WebMouseEvent event(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.SetPositionInWidget(x, y);
    observer_->OnInputEvent(*GetRenderWidgetHost(), event,
                            content::RenderWidgetHost::InputEventObserver::
                                InputEventSource::kUnknown);
    task_environment()->RunUntilIdle();
  }

  void SimulateMouseShake() {
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
    SimulateMouseMove(0.0f, 0.0f);
    SimulateMouseMove(20.0f, 0.0f);
  }
};

TEST_F(GlicSelectionObserverTest, ObserverDeduplicatesRenderWidgetHosts) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  NavigateAndCommit(GURL("http://example.com"));
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();

  // Create a child frame. It should share the RenderWidgetHost with the main
  // frame.
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("child");
  ASSERT_TRUE(child_rfh);
  ASSERT_EQ(main_rfh->GetRenderWidgetHost(), child_rfh->GetRenderWidgetHost());

  // Trigger RenderFrameCreated for both.
  observer->RenderFrameCreated(main_rfh);
  observer->RenderFrameCreated(child_rfh);

  EXPECT_EQ(2u, GetObservedFramesCount());

  // Removing the child frame should remove it from the map, but not the main
  // frame.
  observer->RenderFrameDeleted(child_rfh);
  EXPECT_EQ(1u, GetObservedFramesCount());

  // We can't directly check the internal observer list of RenderWidgetHost
  // without exposing it in test headers, but we can verify our observer's map
  // handles the duplicate RenderWidgetHost correctly.

  observer->RenderFrameDeleted(main_rfh);
  EXPECT_EQ(0u, GetObservedFramesCount());
}

TEST_F(GlicSelectionObserverTest, SelectionClearsInstantly) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Set initial selection (processed instantly when not selecting).
  observer->OnTextSelectionChanged(nullptr, u"Initial");
  EXPECT_EQ(1, observer->update_count());
  observer->Reset();

  // Clear selection.
  observer->OnTextSelectionChanged(nullptr, u"");

  // Clearing is also processed instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, TooLongSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // kMaxSelectionLength is 1000.
  std::u16string huge_text(1001, 'a');
  observer->OnTextSelectionChanged(nullptr, huge_text);

  // Should be treated as clearing (empty text) instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, TooShortSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // kMinSelectionLength is 3.
  std::u16string short_text(2, 'a');
  observer->OnTextSelectionChanged(nullptr, short_text);

  // Should be treated as clearing (empty text) instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, WhitespaceIgnoredInLengthCheck) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // "a b " has 2 non-whitespace characters.
  std::u16string text = u"a b ";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be treated as clearing (empty text) because it has < 3
  // non-whitespace chars.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());

  observer->Reset();

  // "a b c" has 3 non-whitespace characters.
  text = u"a b c";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be accepted.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(text, *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionTrimmed) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // "  abc  " should be trimmed to "abc".
  std::u16string text = u"  abc  ";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be accepted and trimmed.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"abc", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, MultipleSelectionUpdatesDuringDrag) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate MouseDown to start drag.
  blink::WebMouseEvent mouse_down(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // First selection update during drag.
  observer->OnTextSelectionChanged(nullptr, u"First");
  EXPECT_EQ(0, observer->update_count());

  // Second selection update during drag.
  observer->OnTextSelectionChanged(nullptr, u"Second");
  EXPECT_EQ(0, observer->update_count());

  // Simulate MouseUp to complete drag.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify that only the last selection is processed instantly.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Second", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, KeyboardSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Simulate a keyboard event.
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  // Simulate a keyboard event using the WebContents RenderWidgetHost.
  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Send a text selection event representing a collapsed selection.
  observer->OnTextSelectionChanged(nullptr, u"");

  // It should clear the selection immediately.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());

  observer->Reset();

  // Now simulate a mouse down event to clear the keyboard state.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  observer->OnTextSelectionChanged(nullptr, u"Mouse Selection");
  // Should be ignored during selection drag.
  EXPECT_EQ(0, observer->update_count());

  // Simulate a MouseUp event.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Should be processed instantly on MouseUp.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Mouse Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, InlineKeyboardSelectionByShiftAndArrowKeys) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate a Shift and Arrow Right keydown event.
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = ui::VKEY_RIGHT;

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Send a text selection event representing the expanding selection.
  observer->OnTextSelectionChanged(nullptr, u"Keyboard Selection");

  // The selection update should be deferred while actively selecting.
  EXPECT_EQ(0, observer->update_count());

  // Simulate a KeyUp event to complete the inline selection.
  blink::WebKeyboardEvent key_up(
      blink::WebInputEvent::Type::kKeyUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // The final selection should be processed instantly on KeyUp.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Keyboard Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectAllCommandDebouncedProperly) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate a Ctrl+A keydown event.
#if BUILDFLAG(IS_MAC)
  int modifier = blink::WebInputEvent::Modifiers::kMetaKey;
#else
  int modifier = blink::WebInputEvent::Modifiers::kControlKey;
#endif
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, modifier,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = ui::VKEY_A;

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Send a text selection event representing the entire selected page.
  observer->OnTextSelectionChanged(nullptr, u"Entire Page Selected Text");

  // The UI update should be deferred mid-selection command execution.
  EXPECT_EQ(0, observer->update_count());

  // Simulate a KeyUp event to finalize command evaluation.
  blink::WebKeyboardEvent key_up(
      blink::WebInputEvent::Type::kKeyUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // The selected page text should be processed instantly on KeyUp.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Entire Page Selected Text", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, InputEventsDismissUI) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  // Keyboard events should dismiss UI with DismissReason::kExternal.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(nullptr));
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  testing::Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();

  // Mouse clicks should dismiss UI with DismissReason::kExternal.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(nullptr));
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  testing::Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();

  // Scroll events should dismiss UI with DismissReason::kExternal.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface()).Times(0);
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, scroll_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  testing::Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();
}

TEST_F(GlicSelectionObserverTest, PrimaryMainFrameResizedDismissesUI) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  observer->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
}

TEST_F(GlicSelectionObserverTest, OnLinkGeneratedSuccess) {
  GURL fallback_url("https://example.com");
  std::string selector = "test-selector";

  CallOnLinkGenerated(
      fallback_url, selector, shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);

  EXPECT_TRUE(GetGeneratedLink().has_value());
  EXPECT_EQ(GetGeneratedLink().value().spec(),
            "https://example.com/#:~:text=test-selector");
}

TEST_F(GlicSelectionObserverTest, OnLinkGeneratedEmptySelector) {
  GURL fallback_url("https://example.com");
  std::string selector = "";

  CallOnLinkGenerated(
      fallback_url, selector,
      shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);

  EXPECT_FALSE(GetGeneratedLink().has_value());
}

TEST_F(GlicSelectionObserverTest, CopyLinkToHighlight) {
  ui::TestClipboard* clipboard = ui::TestClipboard::CreateForCurrentThread();

  NavigateAndCommit(GURL("https://example.com"));

  GURL fallback_url("https://example.com");
  std::string selector = "test-selector";

  CallOnLinkGenerated(
      fallback_url, selector, shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);

  // Trigger copy to clipboard.
  CallCopyLinkToHighlight(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // Allow clipboard async operations to complete and verify the contents.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    base::test::TestFuture<std::u16string> future;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, std::nullopt,
                        future.GetCallback());
    return base::UTF16ToUTF8(future.Get()) ==
           "https://example.com/#:~:text=test-selector";
  }));

  ui::Clipboard::DestroyClipboardForCurrentThread();
}

TEST_F(GlicSelectionObserverTest, SelectionShowOnlyAfterMouseUp) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate MouseDown.
  blink::WebMouseEvent mouse_down_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Selection changes during drag.
  observer->OnTextSelectionChanged(nullptr, u"Drag Selection");

  // Verify no update has been processed yet.
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(0, observer->update_count());

  // Simulate MouseUp.
  blink::WebMouseEvent mouse_up_event(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify the update has been processed immediately.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Drag Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionShowOnTripleClick) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Simulate one click.
  blink::WebMouseEvent mouse_down1(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down1.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down1.click_count = 1;
  observer->OnInputEvent(*rwh, mouse_down1,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  blink::WebMouseEvent mouse_up1(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up1.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up1.click_count = 1;
  observer->OnInputEvent(*rwh, mouse_up1,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Simulate a double click.
  blink::WebMouseEvent mouse_down2(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down2.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down2.click_count = 2;
  observer->OnInputEvent(*rwh, mouse_down2,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  observer->OnTextSelectionChanged(nullptr, u"Word");

  blink::WebMouseEvent mouse_up2(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up2.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up2.click_count = 2;
  observer->OnInputEvent(*rwh, mouse_up2,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify widget shown for Word.
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Word", *observer->last_processed_text());

  observer->Reset();

  // Simulate a triple click.
  blink::WebMouseEvent mouse_down3(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down3.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down3.click_count = 3;
  observer->OnInputEvent(*rwh, mouse_down3,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  observer->OnTextSelectionChanged(nullptr, u"Entire Paragraph");

  blink::WebMouseEvent mouse_up3(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up3.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up3.click_count = 3;
  observer->OnInputEvent(*rwh, mouse_up3,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify widget shown for Entire Paragraph.
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Entire Paragraph", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionShowOnShiftClick) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Initial mouse selection.
  observer->OnTextSelectionChanged(nullptr, u"Initial Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Initial Text", *observer->last_processed_text());
  observer->Reset();

  // Hold Shift (KeyDown).
  blink::WebKeyboardEvent shift_down(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, shift_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Expect UI to be dismissed.
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(observer->dismiss_ui_reason(),
            GlicSelectionObserver::DismissReason::kExternal);
  observer->Reset();

  // Simulate MouseDown with Shift modifier.
  blink::WebMouseEvent mouse_down(
      blink::WebInputEvent::Type::kMouseDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Selection extended.
  observer->OnTextSelectionChanged(nullptr, u"Initial Text Extended");

  // Simulate MouseUp.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_up,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  task_environment()->RunUntilIdle();

  // Verify widget shown for extended text.
  EXPECT_EQ(1, observer->update_count());
  EXPECT_EQ(u"Initial Text Extended", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, UpdateSelectionStatePanelShowing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt, {{"updates_only", "false"}});

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->show_selection_affordance_called());
  EXPECT_EQ(u"Selected Text", *observer->last_affordance_text());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       UpdateSelectionStatePanelNotShowingGlobalShowHide) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(false);

  // Call UpdateSelectionState directly with is_pending_selection = false,
  // simulating OnGlobalPanelShowHide when the panel is not opened.
  observer->UpdateSelectionState(
      u"Selected Text",
      /*is_pending_selection=*/false,
      GlicSelectionObserver::SelectionSource::kAutomatic);

  EXPECT_FALSE(observer->show_selection_affordance_called());
  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest, EligibleSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Eligible Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Eligible Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest, IneligibleSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Ineligible Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest, DynamicEligibilityChangeClearsContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Eligible Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Eligible Text", *observer->last_sent_context());

  // Simulate eligibility changing to false dynamically.
  observer->Reset();
  SetMockEligibility(false);

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       EligibilityChangePushesContextWhenPanelShowing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  // Selection happens while ineligible and panel is showing.
  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());

  // Eligibility changes to eligible with panel open: context is sent.
  SetMockEligibility(true);
  observer->OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  // We have to reset the selected text here because setting mock eligibility
  // causes a navigation which clears the previous selected text.
  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Selected Text", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       EligibilityChangeDoesNotPushContextWhenPanelClosed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  // Selection happens while ineligible and panel is closed.
  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(false);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());

  // Eligibility changes to eligible with panel closed: context is NOT sent.
  SetMockEligibility(true);
  observer->OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest,
       EligibilityChangeDoesNotPushContextWhenNullBrowserWindow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionPrompt);

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(nullptr));

  SetMockEligibility(false);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Selected Text");
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_FALSE(observer->send_context_called());

  SetMockEligibility(true);
  observer->OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  EXPECT_FALSE(observer->send_context_called());
}

TEST_F(GlicSelectionObserverTest, IdentityManagerIntegration) {
  RecreateObserver();

  // Actually, we can test that the observer's initialization succeeds,
  // and we could potentially check if MockIsEligibleWithAccount receives the
  // right string if we used a more complex mock, but here we just ensure it
  // doesn't crash and returns gracefully when unsigned.
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);
  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));
  observer->OnTextSelectionChanged(nullptr, u"Test text");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Test text", *observer->last_sent_context());

  // Signed in case
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  RecreateObserver();
  observer = GetObserver();
  ASSERT_TRUE(observer);
  SetMockEligibility(true);
  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->OnTextSelectionChanged(nullptr, u"Test text signed in");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"Test text signed in", *observer->last_sent_context());
  observer_.reset();
}

TEST_F(GlicSelectionObserverTest, OnHideHidesSelectionWidget) {
  GURL url("https://example.com");
  NavigateAndCommit(url);
  TestGlicSelectionObserver* observer = GetObserver();
  ASSERT_TRUE(observer);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                url, GURL(), ContentSettingsType::INLINE_CUE_MENU));
  EXPECT_TRUE(ShouldShowSelectionWidget());

  CallOnHide();
  EXPECT_FALSE(ShouldShowSelectionWidget());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(
                url, GURL(), ContentSettingsType::INLINE_CUE_MENU));
}

TEST_F(GlicSelectionObserverTest,
       ShakeTriggerSucceedsWhenFeatureAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicSelectionPrompt,
                            features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  EXPECT_FALSE(observer->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_TRUE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest, ShakeTriggerDisabledByFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicSelectionPrompt},
      /*disabled_features=*/{features::kGlicShakeTrigger});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  EXPECT_FALSE(observer->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_FALSE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest, ShakeTriggerDisabledByPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicSelectionPrompt,
                            features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, false);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  EXPECT_FALSE(observer->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_FALSE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest, ContinuousMoveDoesNotTriggerShake) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicSelectionPrompt,
                            features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Move continuously in the positive X direction.
  SimulateMouseMove(0.0f, 0.0f);
  SimulateMouseMove(20.0f, 0.0f);
  SimulateMouseMove(40.0f, 0.0f);
  SimulateMouseMove(60.0f, 0.0f);
  SimulateMouseMove(80.0f, 0.0f);
  SimulateMouseMove(100.0f, 0.0f);

  EXPECT_FALSE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest, ShakeTriggerDisabledWhenSidePanelClosed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicSelectionPrompt,
                            features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);
  observer->set_mock_side_panel_open(false);

  EXPECT_FALSE(observer->IsShakeTriggerEnabled());

  EXPECT_FALSE(observer->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_FALSE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest,
       ShakeTriggerSucceedsWhenSidePanelClosedIfOnlyOnSidePanelFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kGlicSelectionPrompt, {}},
                            {features::kGlicShakeTrigger,
                             {{"only_on_side_panel", "false"}}}},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);
  observer->set_mock_side_panel_open(false);

  EXPECT_TRUE(observer->IsShakeTriggerEnabled());

  EXPECT_FALSE(observer->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_TRUE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest, ShakeTriggerSucceedsWhenSidePanelOpen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicSelectionPrompt,
                            features::kGlicShakeTrigger},
      /*disabled_features=*/{});
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  NavigateAndCommit(GURL("https://example.com/"));

  auto* observer = GetObserver();
  ASSERT_TRUE(observer);
  observer->set_mock_side_panel_open(true);

  EXPECT_TRUE(observer->IsShakeTriggerEnabled());

  EXPECT_FALSE(observer->trigger_region_capture_called());
  SimulateMouseShake();
  EXPECT_TRUE(observer->trigger_region_capture_called());
}

TEST_F(GlicSelectionObserverTest, BaseIsSidePanelOpenReturnsFalseWithoutTab) {
  EXPECT_FALSE(observer_->BaseIsSidePanelOpen());
}

TEST_F(GlicSelectionObserverTest, SelectionWordCountMetrics) {
  base::HistogramTester histogram_tester;

  std::u16string text = u"   one   two\nthree\t ";
  InvokeGlicFromSelectionAffordance(text, /*is_widget=*/true,
                                    web_contents()->GetWeakPtr());

  histogram_tester.ExpectUniqueSample(
      "Glic.Selection.WidgetClicked.SelectionLength.PreFre", text.length(), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Selection.WidgetClicked.SelectionWordCount.PreFre", 3, 1);
}

class GlicSelectionObserverPromptTest : public GlicSelectionObserverTest {
 public:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/true);
    GlicSelectionObserverTest::SetUp();
    observer_.reset();

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&GlicSelectionObserverPromptTest::CreateService,
                            base::Unretained(this)));

    GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
    RecreateObserver();
  }

  void TearDown() override {
    observer_.reset();
    mock_service_ = nullptr;
    GlicSelectionObserverTest::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_service_ = service.get();
    return service;
  }

  MockGlicKeyedService* mock_glic_service() { return mock_service_; }

 protected:
  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
};

TEST_F(GlicSelectionObserverPromptTest,
       InvokeGlicFromSelectionAffordanceExplainCta) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt,
        {{"auto_send_prompt", "true"}, {"cta", "explain"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(
      *mock_glic_service(),
      InvokeWithAutoSubmit(
          testing::_,
          testing::Field(&GlicInvokeOptions::prompts,
                         testing::ElementsAre(l10n_util::GetStringUTF8(
                             IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_EXPLAIN)))))
      .Times(1);

  InvokeGlicFromSelectionAffordance(u"Sample selected text", /*is_widget=*/true,
                                    web_contents()->GetWeakPtr());
}

TEST_F(GlicSelectionObserverPromptTest,
       InvokeGlicFromSelectionAffordanceTellMeAboutThisCta) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt,
        {{"auto_send_prompt", "true"}, {"cta", "tell_me_about_this"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(
      *mock_glic_service(),
      InvokeWithAutoSubmit(
          testing::_,
          testing::Field(&GlicInvokeOptions::prompts,
                         testing::ElementsAre(l10n_util::GetStringUTF8(
                             IDS_GLIC_SELECTION_AUTO_SEND_PROMPT_TELL_ME)))))
      .Times(1);

  InvokeGlicFromSelectionAffordance(u"Sample selected text", /*is_widget=*/true,
                                    web_contents()->GetWeakPtr());
}

TEST_F(GlicSelectionObserverPromptTest,
       InvokeGlicFromSelectionAffordancePromptOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt,
        {{"auto_send_prompt", "true"}, {"cta", "explain"}}}},
      {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(*mock_glic_service(),
              InvokeWithAutoSubmit(
                  testing::_,
                  testing::Field(&GlicInvokeOptions::prompts,
                                 testing::ElementsAre(
                                     "Tell me more about \"Sample text\""))))
      .Times(1);

  InvokeGlicFromSelectionAffordance(
      u"Sample text", /*is_widget=*/true, web_contents()->GetWeakPtr(),
      /*prompt_override=*/u"Tell me more about \"Sample text\"");
}

TEST_F(GlicSelectionObserverPromptTest,
       InvokeGlicFromSelectionAffordanceAutoSendDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicSelectionPrompt, {{"auto_send_prompt", "false"}}}}, {});

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  EXPECT_CALL(
      *mock_glic_service(),
      Invoke(testing::Field(&GlicInvokeOptions::prompts, testing::IsEmpty())))
      .Times(1);

  InvokeGlicFromSelectionAffordance(u"Sample selected text", /*is_widget=*/true,
                                    web_contents()->GetWeakPtr());
}

TEST_F(GlicSelectionObserverTest, ShouldShowSelectionWidgetSiteBlocked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{features::kGlicSelectionDefaultBlockedSites.name,
        "https://blocked-site.com"}});

  NavigateAndCommit(GURL("https://blocked-site.com/page"));
  EXPECT_FALSE(observer_->ShouldShowSelectionWidget());

  NavigateAndCommit(GURL("https://allowed-site.com/page"));
  EXPECT_TRUE(observer_->ShouldShowSelectionWidget());

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(GURL("https://allowed-site.com/page"),
                                      GURL("https://allowed-site.com/page"),
                                      ContentSettingsType::INLINE_CUE_MENU,
                                      CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(observer_->ShouldShowSelectionWidget());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnClickAway) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Set selection context sent (e.g. from context menu invocation in editable
  // or non-editable field).
  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Simulate left click down on the page (clicking away).
  blink::WebMouseEvent mouse_down(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_down.button = blink::WebPointerProperties::Button::kLeft;
  mouse_down.click_count = 1;
  observer->OnInputEvent(*rwh, mouse_down,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !observer->has_sent_selection_context(); }));

  // Verify that the selection context has been cleared and empty context sent.
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnTextSelectionChanged) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Simulate keyboard/programmatic deselection.
  observer->OnTextSelectionChanged(nullptr, u"");

  // Verify that the selection context has been cleared and empty context sent.
  EXPECT_FALSE(observer->has_sent_selection_context());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnUnshareablePageDeselection) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  // Navigate to an unshareable chrome:// URL.
  NavigateAndCommit(GURL("chrome://version"));

  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Simulate deselection on unshareable page.
  observer->OnTextSelectionChanged(nullptr, u"");

  EXPECT_FALSE(observer->has_sent_selection_context());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest,
       SetHasSentSelectionContextClearedOnPrimaryPageChanged) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  tabs::MockTabInterface mock_tab;
  MockBrowserWindowInterface mock_bwi;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_bwi));

  observer->set_call_base_update_selection_state(true);
  observer->set_mock_panel_showing(true);

  observer->UpdateSelectionStateFromContextMenu(u"Selected text context");
  EXPECT_TRUE(observer->has_sent_selection_context());

  // Navigate to a new primary page.
  NavigateAndCommit(GURL("https://example.com/page2"));

  // Verify that navigating cleared the selection context and notified the
  // panel.
  EXPECT_FALSE(observer->has_sent_selection_context());
  EXPECT_TRUE(observer->send_context_called());
  EXPECT_EQ(u"", *observer->last_sent_context());
}

TEST_F(GlicSelectionObserverTest, OnAskGeminiWithSmallChipDismissesUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSelectionSmallChip);

  TestGlicSelectionObserver* observer = GetObserver();
  ASSERT_TRUE(observer);

  CallOnAskGemini();

  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_EQ(GlicSelectionObserver::DismissReason::kActionTaken,
            observer->dismiss_ui_reason());
  EXPECT_TRUE(observer->show_selection_overlay_called());
}

}  // namespace glic
