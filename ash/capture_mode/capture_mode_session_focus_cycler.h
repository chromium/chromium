// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_FOCUS_CYCLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_FOCUS_CYCLER_H_

#include <cstddef>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class FocusRing;
class HighlightPathGenerator;
class View;
class Widget;
}  // namespace views

namespace ash {

class CaptureModeSession;
class ScopedA11yOverrideWindowSetter;

// CaptureModeSessionFocusCycler handles the special focus transitions which
// happen between the capture session UI items. These include the capture bar
// buttons, the selection region UI and the capture button.
// TODO(crbug.com/40170806): The selection region UI are drawn directly on a
// layer. We simulate focus by drawing focus rings on the same layer, but this
// is not compatible with accessibility. Investigate using AxVirtualView or
// making the dots actual Views.
class ASH_EXPORT CaptureModeSessionFocusCycler : public views::WidgetObserver {
 public:
  // The different groups which can receive focus during a capture mode session.
  // A group may have multiple items which can receive focus.
  // TODO(crbug.com/40170806): Investigate removing the groups concept and
  // having one flat list.
  enum class FocusGroup {
    kNone = 0,
    // The buttons to select the capture type and source on the capture bar.
    kTypeSource,
    // The start recording button inside the game capture bar.
    kStartRecordingButton,
    // In region mode, the UI to adjust a partial region.
    kSelection,
    // The button in the middle of a selection region to capture or record.
    kCaptureButton,
    // In window mode, the group to tab through the available MRU windows.
    kCaptureWindow,
    // The buttons to open the settings menu and exit capture mode on the
    // capture bar.
    kSettingsClose,
    // A state where nothing is visibly focused. The next focus will advance to
    // the settings menu. This is to match tab focusing on other menus such as
    // quick settings.
    kPendingSettings,
    // The buttons inside the settings menu.
    kSettingsMenu,
    // The camera preview shown inside the current capture surface.
    kCameraPreview,
    // Similar to `kPendingSettings` above, but for the recording type menu.
    kPendingRecordingType,
    // The menu items inside the recording type menu.
    kRecordingTypeMenu,
  };

  // If a focusable capture session item is part of a views hierarchy, it needs
  // to override this class, which manages a custom focus ring.
  class HighlightableView {
   public:
    bool has_focus() const { return has_focus_; }

    // Get the view class associated with |this|.
    virtual views::View* GetView() = 0;

    // Subclasses can override this for a custom focus ring shape. Defaults to
    // return nullptr, which means the focus ring will use the
    // HighlightPathGenerator used for clipping ink drops.
    virtual std::unique_ptr<views::HighlightPathGenerator>
    CreatePathGenerator();

    // Sets `needs_highlight_path_` to true, so that a new highlight path
    // generator can be created and installed on the focus ring the next time
    // `PseudoFocus()` is called.
    void InvalidateFocusRingPath();

    // Shows the focus ring and triggers setting accessibility focus on the
    // associated view.
    virtual void PseudoFocus();

    // Hides the focus ring.
    virtual void PseudoBlur();

    // Attempt to mimic a click on the associated view. Called by
    // CaptureModeSession when it receives a space, or enter key events, as the
    // button is not actuallly focused and will do nothing otherwise. Triggers
    // the button handler if the view is a subclass of Button, and returns true.
    // Does nothing otherwise and returns false.
    virtual bool ClickView();

   protected:
    HighlightableView();
    virtual ~HighlightableView();

    // TODO(crbug.com/40170806): This can result in multiple of these objects
    // thinking they have focus if CaptureModeSessionFocusCycler does not call
    // PseudoFocus or PseudoBlur properly. Investigate if there is a better
    // approach.
    bool has_focus_ = false;

   private:
    // A convenience pointer to the focus ring, which is owned by the views
    // hierarchy.
    raw_ptr<views::FocusRing, DanglingUntriaged> focus_ring_ = nullptr;

    // True until a highlight path generator has been installed on the focus
    // ring. The path generator can be refreshed (e.g. to change the shape of
    // the focus ring) via calling `InvalidateFocusRingPath()`, which will set
    // this to back to `true`.
    bool needs_highlight_path_ = true;

    base::WeakPtrFactory<HighlightableView> weak_ptr_factory_{this};
  };

  // An aura window that can be focused in capture session.
  class HighlightableWindow : public HighlightableView,
                              public aura::WindowObserver {
   public:
    HighlightableWindow(aura::Window* window, CaptureModeSession* session);
    HighlightableWindow(const HighlightableWindow&) = delete;
    HighlightableWindow& operator=(const HighlightableWindow&) = delete;
    ~HighlightableWindow() override;

    // HighlightableView:
    views::View* GetView() override;
    void PseudoFocus() override;
    void PseudoBlur() override;
    bool ClickView() override;

    // aura::WindowObserver:
    void OnWindowDestroying(aura::Window* window) override;

   private:
    const raw_ptr<aura::Window> window_;
    const raw_ptr<CaptureModeSession> session_;
  };

  // Defines a type for a callback that can be called to construct a highlight
  // path generator which will be used for a custom focus ring shape.
  using HighlightPathGeneratorFactory =
      base::RepeatingCallback<std::unique_ptr<views::HighlightPathGenerator>()>;

  // A helper class that creates a highlightable object for a given view. The
  // helper is mainly used for the views that need to be created by other
  // classes, such as the `IconButton` created by `IconSwitch`.
  class HighlightHelper
      : public CaptureModeSessionFocusCycler::HighlightableView {
   public:
    explicit HighlightHelper(views::View* view);
    HighlightHelper(views::View* view, HighlightPathGeneratorFactory callback);
    HighlightHelper(const HighlightHelper&) = delete;
    HighlightHelper& operator=(const HighlightHelper&) = delete;
    ~HighlightHelper() override;

    static void Install(views::View* view);
    static void Install(views::View* view,
                        HighlightPathGeneratorFactory callback);
    static HighlightHelper* Get(views::View* view);

    // CaptureModeSessionFocusCycler::HighlightableView:
    views::View* GetView() override;
    std::unique_ptr<views::HighlightPathGenerator> CreatePathGenerator()
        override;

   private:
    const raw_ptr<views::View> view_;
    HighlightPathGeneratorFactory highlight_path_generator_factory_;
  };

  explicit CaptureModeSessionFocusCycler(CaptureModeSession* session);
  CaptureModeSessionFocusCycler(const CaptureModeSessionFocusCycler&) = delete;
  CaptureModeSessionFocusCycler& operator=(
      const CaptureModeSessionFocusCycler&) = delete;
  ~CaptureModeSessionFocusCycler() override;

  // Advances the focus by simulating focus on a view, or updating the
  // CaptureModeSession to draw focus on elements which are not associated with
  // a view. The order is as follows:
  //   1) Capture mode type and source: (Screenshot/recording) and
  //      (fullscreen/region/window) on the capture bar.
  //   2) Region selection area: If visible.
  //   3) Capture/record button: If visible.
  //   4) Recording type menu: If visible.
  //   5) Settings menu: If visible.
  //   6) Settings and close button: On the capture bar.
  // This should be called by CaptureModeSession when it receives a VKEY_TAB.
  void AdvanceFocus(bool reverse);

  // Removes focus. Called by CaptureModeSession when it receives a VKEY_ESC, or
  // when the state has changed such that a currently focus item is invalid
  // (i.e. switching from region mode to windowed mode makes a focused selection
  // or capture button invalid).
  void ClearFocus();

  // Returns true if anything has focus.
  bool HasFocus() const;

  // Activates the currently focused view (if any) (e.g. by pressing a button if
  // the focused view is a button). If the given `ignore_view` is the currently
  // focused view, it does nothing and returns false. Returns true if the
  // focused view should take the event; when this happens the
  // CaptureModeSession should not handle the event.
  bool MaybeActivateFocusedView(views::View* ignore_view);

  // Returns true if the current focus group is associated with the UI used for
  // displaying a region.
  bool RegionGroupFocused() const;

  // Returns true if the current focus group is associated with capture bar,
  // otherwise returns false.
  bool CaptureBarFocused() const;

  // Returns true if the current focus is on capture label, otherwise returns
  // false.
  bool CaptureLabelFocused() const;

  // Gets the current fine tune position for drawing the focus rects/rings on
  // the session's layer. Returns FineTunePosition::kNone if focus is on another
  // group.
  FineTunePosition GetFocusedFineTunePosition() const;

  // Called when the capture label widget is made visible or hidden, or changes
  // states. If the label button is visible, it should be on the a11y annotation
  // cycle, otherwise it should be removed from the a11y annotation cycle.
  void OnCaptureLabelWidgetUpdated();

  // Called when either the settings or the recording type menus `widget`'s are
  // opened to set up the focus state. The given `focus_group` will be set as
  // the `current_focus_group_`. If `by_key_event` is true, it means the menu
  // was opened via keyboard navigation, and therefore future calls to
  // `AdvanceFocus()` will navigate to items within the menu, rather than
  // closing the menu.
  void OnMenuOpened(views::Widget* widget,
                    FocusGroup focus_group,
                    bool by_key_event);

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class CaptureModeSessionTestApi;

  // Removes the focus ring from the current focused item if possible. Does not
  // alter |current_focus_group_| or |focus_index_|.
  void ClearCurrentVisibleFocus();

  // Get the next group in the focus order.
  FocusGroup GetNextGroup(bool reverse) const;

  // Returns the current focus group list. It will be one of
  // `groups_for_fullscreen_`, `groups_for_region_` and `groups_for_window_`,
  // depending on the current capture source.
  const std::vector<FocusGroup>& GetCurrentGroupList() const;

  // Returns true if the given `group` is available inside the focus group list
  // returned from GetCurrentGroupList().
  bool IsGroupAvailable(FocusGroup group) const;

  // Returns the number of elements in a particular group.
  size_t GetGroupSize(FocusGroup group) const;

  // Returns the items of a particular |group|. Returns an empty array for the
  // kSelection group as the items in that group do not have associated views.
  std::vector<HighlightableView*> GetGroupItems(FocusGroup group) const;

  // Updates the capture mode widgets so that accessibility features can
  // traverse between our widgets.
  void UpdateA11yAnnotation();

  views::Widget* GetSettingsMenuWidget() const;
  views::Widget* GetRecordingTypeMenuWidget() const;

  // Returns the window which is supposed to be set as the a11y override window
  // for accessibility controller according to the `current_focus_group_`.
  aura::Window* GetA11yOverrideWindow() const;

  // Returns true if there's a focused view in the given `views`, otherwise
  // returns false. In the meanwhile, updates `focus_index_` according to the
  // index of the current focused view to ensure it is up to date.
  bool FindFocusedViewAndUpdateFocusIndex(
      std::vector<HighlightableView*> views);

  // Highlights the corresponding HighlightableWindow first before moving the
  // focus on the items inside. This happens when current focus group is
  // `kCaptureWindow`, we need to focus the window first to update it as the
  // current selected window. Thus the camera preview can be shown inside the
  // updated selected window.
  void MaybeFocusHighlightableWindow(
      const std::vector<HighlightableView*>& current_views);

  // The current focus group and focus index.
  FocusGroup current_focus_group_ = FocusGroup::kNone;
  size_t focus_index_ = 0u;

  // Focusable groups for each capture source.
  const std::vector<FocusGroup> groups_for_fullscreen_;
  const std::vector<FocusGroup> groups_for_region_;
  const std::vector<FocusGroup> groups_for_window_;

  // Focusable groups for the game capture session that always has `kWindow`
  // capture source selected. And the selected window is not changeable.
  const std::vector<FocusGroup> groups_for_game_capture_;

  // Highlightable windows of the focus group `kCaptureWindow`. Windows opened
  // after the session starts will not be included.
  std::map<aura::Window*, std::unique_ptr<HighlightableWindow>>
      highlightable_windows_;

  // The session that owns |this|. Guaranteed to be non null for the lifetime of
  // |this|.
  raw_ptr<CaptureModeSession> session_;

  // Accessibility features will focus on whatever window is returned by
  // GetA11yOverrideWindow(). Once `this` goes out of scope, the a11y override
  // window is set to null.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      menu_widget_observeration_{this};

  // True if the current open menu (either settings or recording type) was open
  // by a key event (e.g. spacebar press) on their entry point button. In that
  // case, `AdvanceFocus()` will navigate to items within that menu. Otherwise,
  // `AdvanceFocus()` will close the menu.
  bool menu_opened_with_keyboard_nav_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_FOCUS_CYCLER_H_
