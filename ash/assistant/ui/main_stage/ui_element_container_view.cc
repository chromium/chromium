// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include <string>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFirstCardMarginTopDip = 40;
constexpr int kPaddingBottomDip = 24;

// Card element animation.
constexpr float kCardElementAnimationFadeOutOpacity = 0.26f;

// Text element animation.
constexpr float kTextElementAnimationFadeOutOpacity = 0.f;

// UI element animation.
constexpr base::TimeDelta kUiElementAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

// Helpers ---------------------------------------------------------------------

void CreateAndSendMouseClick(aura::WindowTreeHost* host,
                             const gfx::Point& location_in_pixels) {
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, location_in_pixels,
                             location_in_pixels, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

  // Send an ET_MOUSE_PRESSED event.
  ui::EventDispatchDetails details =
      host->event_sink()->OnEventFromSource(&press_event);

  if (details.dispatcher_destroyed)
    return;

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, location_in_pixels,
                               location_in_pixels, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);

  // Send an ET_MOUSE_RELEASED event.
  ignore_result(host->event_sink()->OnEventFromSource(&release_event));
}

// CardElementViewHolder -------------------------------------------------------

// TODO(dmblack): Move this class to standalone file as part of clean up effort.
// This class uses a child widget to host a view for a card element that has an
// aura::Window. The child widget's layer becomes the root of the card's layer
// hierarchy.
class CardElementViewHolder : public views::NativeViewHost,
                              public views::ViewObserver {
 public:
  explicit CardElementViewHolder(const AssistantCardElement* card_element)
      : card_element_view_(app_list::AnswerCardContentsRegistry::Get()->GetView(
            card_element->embed_token().value())) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);

    params.name = GetClassName();
    params.delegate = new views::WidgetDelegateView();
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    child_widget_ = std::make_unique<views::Widget>();
    child_widget_->Init(params);

    contents_view_ = params.delegate->GetContentsView();
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    contents_view_->AddChildView(card_element_view_);

    card_element_view_->AddObserver(this);

    // OverrideDescription() doesn't work. Only names are read automatically.
    GetViewAccessibility().OverrideName(card_element->fallback());
  }

  ~CardElementViewHolder() override {
    if (card_element_view_)
      card_element_view_->RemoveObserver(this);
  }

  // views::NativeViewHost:
  const char* GetClassName() const override { return "CardElementViewHolder"; }

  void OnGestureEvent(ui::GestureEvent* event) override {
    // We need to route GESTURE_TAP events to our Assistant card because links
    // should be tappable. The Assistant card window will not receive gesture
    // events so we convert the gesture into analogous mouse events.
    if (event->type() != ui::ET_GESTURE_TAP) {
      views::View::OnGestureEvent(event);
      return;
    }

    // Consume the original event.
    event->StopPropagation();
    event->SetHandled();

    aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();

    // Get the appropriate event location in pixels.
    gfx::Point location_in_pixels = event->location();
    ConvertPointToScreen(this, &location_in_pixels);
    aura::WindowTreeHost* host = root_window->GetHost();
    host->ConvertDIPToPixels(&location_in_pixels);

    wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();

    // We want to prevent the cursor from changing its visibility during our
    // mouse events because we are actually handling a gesture. To accomplish
    // this, we cache the cursor's visibility and lock it in its current state.
    const bool visible = cursor_manager->IsCursorVisible();
    cursor_manager->LockCursor();

    CreateAndSendMouseClick(host, location_in_pixels);

    // Restore the original cursor visibility that may have changed during our
    // sequence of mouse events. This change would not have been perceivable to
    // the user since it occurred within our lock.
    if (visible)
      cursor_manager->ShowCursor();
    else
      cursor_manager->HideCursor();

    // Release our cursor lock.
    cursor_manager->UnlockCursor();
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override {
    DCHECK_EQ(card_element_view_, view);

    // It's possible for |card_element_view_| to be destroyed before
    // CardElementViewHolder. When this happens, we need to perform clean up
    // prior to |card_element_view_| being destroyed and remove our cached
    // reference to prevent additional clean up attempts on the destroyed
    // instance when destroying CardElementViewHolder.
    card_element_view_->RemoveObserver(this);
    card_element_view_ = nullptr;
  }

  void OnViewPreferredSizeChanged(views::View* view) override {
    DCHECK_EQ(card_element_view_, view);

    gfx::Size preferred_size = view->GetPreferredSize();

    if (border()) {
      // When a border is present we need to explicitly account for it in our
      // size calculations by enlarging our preferred size by the border insets.
      const gfx::Insets insets = border()->GetInsets();
      preferred_size.Enlarge(insets.width(), insets.height());
    }

    contents_view_->SetPreferredSize(preferred_size);
    SetPreferredSize(preferred_size);
  }

  void Attach() {
    views::NativeViewHost::Attach(child_widget_->GetNativeView());

    aura::Window* const top_level_window = native_view()->GetToplevelWindow();

    // Find the window for the Assistant card.
    aura::Window* window = native_view();
    while (window->parent() != top_level_window)
      window = window->parent();

    // The Assistant card window will consume all events that enter it. This
    // prevents us from being able to scroll the native view hierarchy
    // vertically. As such, we need to prevent the Assistant card window from
    // receiving events it doesn't need. It needs mouse click events for
    // handling links.
    AssistantContainerView::OnlyAllowMouseClickEvents(window);
  }

 private:
  views::View* card_element_view_;  // Owned by WebContentsManager.

  std::unique_ptr<views::Widget> child_widget_;

  views::View* contents_view_ = nullptr;  // Owned by |child_widget_|.

  DISALLOW_COPY_AND_ASSIGN(CardElementViewHolder);
};

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      ui_elements_exit_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_ended_callback=*/base::BindRepeating(
                  &UiElementContainerView::OnAllUiElementsExitAnimationEnded,
                  base::Unretained(this)))) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // UiElementContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

UiElementContainerView::~UiElementContainerView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

const char* UiElementContainerView::GetClassName() const {
  return "UiElementContainerView";
}

gfx::Size UiElementContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int UiElementContainerView::GetHeightForWidth(int width) const {
  return content_view()->GetHeightForWidth(width);
}

void UiElementContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_height = content_view->GetHeightForWidth(width());
  content_view->SetSize(gfx::Size(width(), preferred_height));
}

void UiElementContainerView::PreferredSizeChanged() {
  // Because views are added/removed in batches, we attempt to prevent over-
  // propagation of the PreferredSizeChanged event during batched view hierarchy
  // add/remove operations. This helps to reduce layout passes.
  if (propagate_preferred_size_changed_)
    AssistantScrollView::PreferredSizeChanged();
}

void UiElementContainerView::InitLayout() {
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kUiElementHorizontalMarginDip, kPaddingBottomDip,
                  kUiElementHorizontalMarginDip),
      kSpacingDip));
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // We don't allow processing of events while waiting for the next query
  // response. The contents will be faded out, so it should not be interactive.
  // We also scroll to the top to play nice with the transition animation.
  set_can_process_events_within_subtree(false);
  ScrollToPosition(vertical_scroll_bar(), 0);

  // When a query is committed, we fade out the views for the previous response
  // until the next Assistant response has been received.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(CreateOpacityElement(
            /*opacity=*/pair.second, kUiElementAnimationFadeOutDuration)));
  }
}

void UiElementContainerView::OnResponseChanged(
    const std::shared_ptr<AssistantResponse>& response) {
  // We may have to pend the response while we animate the previous response off
  // stage. We use a shared pointer to ensure that any views we add to the view
  // hierarchy can be removed before the underlying UI elements are destroyed.
  pending_response_ = std::shared_ptr<const AssistantResponse>(response);

  // If we don't have any pre-existing content, there is nothing to animate off
  // stage so we we can proceed to add the new response.
  if (!content_view()->has_children()) {
    OnResponseAdded(std::move(pending_response_));
    return;
  }

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::StartLayerAnimationSequence;

  // There is a previous response on stage, so we'll animate it off before
  // adding the new response. The new response will be added upon invocation of
  // the exit animation ended callback.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    StartLayerAnimationSequence(
        pair.first->layer()->GetAnimator(),
        // Fade out the opacity to 0%. Note that we approximate 0% by actually
        // using 0.01%. We do this to workaround a DCHECK that requires
        // aura::Windows to have a target opacity > 0% when shown. Because our
        // window will be removed after it reaches this value, it should be safe
        // to circumnavigate this DCHECK.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.0001f, kUiElementAnimationFadeOutDuration)),
        // Observe the animation.
        ui_elements_exit_animation_observer_.get());
  }

  // Set the observer to active so that we receive callback events.
  ui_elements_exit_animation_observer_->SetActive();
}

void UiElementContainerView::OnResponseCleared() {
  // We need to detach native view hosts before they are removed from the view
  // hierarchy and destroyed.
  if (!native_view_hosts_.empty()) {
    for (views::NativeViewHost* native_view_host : native_view_hosts_)
      native_view_host->Detach();
    native_view_hosts_.clear();
  }

  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  SetPropagatePreferredSizeChanged(false);
  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  ui_element_views_.clear();
  SetPropagatePreferredSizeChanged(true);

  // Once the response has been cleared from the stage, we can are free to
  // release our shared pointer. This allows resources associated with the
  // underlying UI elements to be freed, provided there are no other usages.
  response_.reset();

  // Reset state for the next response.
  is_first_card_ = true;
}

void UiElementContainerView::OnResponseAdded(
    std::shared_ptr<const AssistantResponse> response) {
  // The response should be fully processed before it is presented.
  DCHECK_EQ(AssistantResponse::ProcessingState::kProcessed,
            response->processing_state());

  // We cache a reference to the |response| to ensure that the instance is not
  // destroyed before we have removed associated views from the view hierarchy.
  response_ = std::move(response);

  // Because the views for the response are animated in together, we can stop
  // propagation of PreferredSizeChanged events until all views have been added
  // to the view hierarchy to reduce layout passes.
  SetPropagatePreferredSizeChanged(false);

  for (const auto& ui_element : response_->GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        OnCardElementAdded(
            static_cast<const AssistantCardElement*>(ui_element.get()));
        break;
      case AssistantUiElementType::kText:
        OnTextElementAdded(
            static_cast<const AssistantTextElement*>(ui_element.get()));
        break;
    }
  }

  OnAllUiElementsAdded();
}

void UiElementContainerView::OnCardElementAdded(
    const AssistantCardElement* card_element) {
  // The card, for some reason, is not embeddable so we'll have to ignore it.
  if (!card_element->embed_token().has_value())
    return;

  // When the card has been rendered in the same process, its view is
  // available in the AnswerCardContentsRegistry's token-to-view map.
  if (app_list::AnswerCardContentsRegistry::Get()) {
    auto* view_holder = new CardElementViewHolder(card_element);

    if (is_first_card_) {
      is_first_card_ = false;

      // The first card requires a top margin of |kFirstCardMarginTopDip|, but
      // we need to account for child spacing because the first card is not
      // necessarily the first UI element.
      const int top_margin_dip = child_count() == 0
                                     ? kFirstCardMarginTopDip
                                     : kFirstCardMarginTopDip - kSpacingDip;

      // We effectively create a top margin by applying an empty border.
      view_holder->SetBorder(views::CreateEmptyBorder(top_margin_dip, 0, 0, 0));
    }

    content_view()->AddChildView(view_holder);
    view_holder->Attach();

    // Cache a reference to the attached native view host so that it can be
    // detached prior to being removed from the view hierarchy and destroyed.
    native_view_hosts_.push_back(view_holder);

    // The view will be animated on its own layer, so we need to do some initial
    // layer setup. We're going to fade the view in, so hide it. Note that we
    // approximate 0% opacity by actually using 0.01%. We do this to workaround
    // a DCHECK that requires aura::Windows to have a target opacity > 0% when
    // shown. Because our window will be animated to full opacity from this
    // value, it should be safe to circumnavigate this DCHECK.
    view_holder->native_view()->layer()->SetFillsBoundsOpaquely(false);
    view_holder->native_view()->layer()->SetOpacity(0.0001f);

    // We cache the native view for use during animations and its desired
    // opacity that we'll animate to while processing the next query response.
    ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
        view_holder->native_view(), kCardElementAnimationFadeOutOpacity));
  }

  // TODO(dmblack): Handle Mash case.
}

void UiElementContainerView::OnTextElementAdded(
    const AssistantTextElement* text_element) {
  views::View* text_element_view = new AssistantTextElementView(text_element);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  text_element_view->SetPaintToLayer();
  text_element_view->layer()->SetFillsBoundsOpaquely(false);
  text_element_view->layer()->SetOpacity(0.f);

  // We cache the view for use during animations and its desired opacity that
  // we'll animate to while processing the next query response.
  ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
      text_element_view, kTextElementAnimationFadeOutOpacity));

  content_view()->AddChildView(text_element_view);
}

void UiElementContainerView::OnAllUiElementsAdded() {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // Now that the response for the current query has been added to the view
  // hierarchy, we can re-enable processing of events. We can also restart
  // propagation of PreferredSizeChanged events since all views have been added
  // to the view hierarchy.
  set_can_process_events_within_subtree(true);
  SetPropagatePreferredSizeChanged(true);

  // Now that we've received and added all UI elements for the current query
  // response, we can animate them in.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    // We fade in the views to full opacity after a slight delay.
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kUiElementAnimationFadeInDelay),
            CreateOpacityElement(1.f, kUiElementAnimationFadeInDuration)));
  }

  // Let screen reader read the query result. This includes the text response
  // and the card fallback text, but webview result is not included.
  // We don't read when there is TTS to avoid speaking over the server response.
  const AssistantResponse* response =
      assistant_controller_->interaction_controller()->model()->response();
  if (!response->has_tts())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

bool UiElementContainerView::OnAllUiElementsExitAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // All UI elements have finished their exit animations so its safe to perform
  // clearing of their views and managed resources.
  OnResponseCleared();

  // It is safe to add our pending response to the view hierarchy now that we've
  // cleared the previous response from the stage.
  OnResponseAdded(std::move(pending_response_));

  // Return false to prevent the observer from destroying itself.
  return false;
}

void UiElementContainerView::SetPropagatePreferredSizeChanged(bool propagate) {
  if (propagate == propagate_preferred_size_changed_)
    return;

  propagate_preferred_size_changed_ = propagate;

  // When we are no longer stopping propagation of PreferredSizeChanged events,
  // we fire an event off to ensure the view hierarchy is properly laid out.
  if (propagate_preferred_size_changed_)
    PreferredSizeChanged();
}

}  // namespace ash
