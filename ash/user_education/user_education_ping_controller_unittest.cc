// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_ping_controller.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_class_properties.h"
#include "ash/user_education/user_education_types.h"
#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using ::base::test::RunClosure;
using ::testing::AllOf;
using ::testing::Conditional;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;

// Helpers ---------------------------------------------------------------------

gfx::Rect Inset(const gfx::Rect& rect, const gfx::Insets& insets) {
  gfx::Rect inset_rect(rect);
  inset_rect.Inset(insets);
  return inset_rect;
}

gfx::Rect Inset(const gfx::Rect& rect, const gfx::Insets* insets) {
  return insets ? Inset(rect, *insets) : rect;
}

// MockLayerObserver -----------------------------------------------------------

class MockLayerObserver : public ui::LayerObserver {
 public:
  // ui::LayerObserver:
  MOCK_METHOD(void, LayerDestroyed, (ui::Layer * layer), (override));
};

}  // namespace

// UserEducationPingControllerTest ---------------------------------------------

// Base class for tests of the `UserEducationPingController`.
class UserEducationPingControllerTest : public UserEducationAshTestBase {
 public:
  UserEducationPingControllerTest() {
    // NOTE: The `UserEducationPingController` exists only when a user education
    // feature is enabled. Controller existence is verified in test coverage for
    // the controller's owner.
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.emplace_back(features::kHoldingSpaceWallpaperNudge);
    enabled_features.emplace_back(features::kWelcomeTour);
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  // Returns the singleton instance owned by the `UserEducationController`.
  UserEducationPingController* controller() {
    return UserEducationPingController::Get();
  }

  // Returns the view to ping.
  views::View* view() { return widget_->GetContentsView(); }

  // Creates a ping for the specified view, returning `true` if successful.
  bool CreatePing(PingId ping_id = PingId::kTest1,
                  const std::optional<views::View*>& v = std::nullopt) {
    return controller()->CreatePing(ping_id, v.value_or(this->view()));
  }

  // Expects that no ping exists for the specified view. No ping exists for a
  // view if the view does not have any associated ping layers.
  void ExpectNoPing(const std::optional<views::View*>& v = std::nullopt) {
    views::View* const view = v.value_or(this->view());
    EXPECT_THAT(
        view->GetLayersInOrder(),
        Conditional(view->layer(), ElementsAre(Eq(view->layer())), IsEmpty()));
  }

  // Asserts that a ping exists for the specified view. A ping exists for a view
  // if the view has ping layers configured with expected properties.
  void AssertPingProperties(
      const std::optional<views::View*>& v = std::nullopt) {
    views::View* const view = v.value_or(this->view());
    ASSERT_THAT(
        view->GetLayersInOrder(),
        ElementsAre(
            AllOf(
                Property(&ui::Layer::name,
                         Eq(UserEducationPingController::kPingParentLayerName)),
                Property(&ui::Layer::bounds, Eq(view->layer()->bounds())),
                Property(&ui::Layer::type, Eq(ui::LAYER_NOT_DRAWN)),
                Property(
                    &ui::Layer::children,
                    ElementsAre(AllOf(
                        Property(&ui::Layer::name,
                                 Eq(UserEducationPingController::
                                        kPingChildLayerName)),
                        Property(&ui::Layer::background_color,
                                 Eq(DarkLightModeController::Get()
                                            ->IsDarkModeEnabled()
                                        ? SK_ColorWHITE
                                        : SK_ColorBLACK)),
                        Property(
                            &ui::Layer::bounds,
                            Eq(Inset(gfx::Rect(view->layer()->bounds().size()),
                                     view->GetProperty(kPingInsetsKey)))),
                        Property(
                            &ui::Layer::rounded_corner_radii,
                            Eq(gfx::RoundedCornersF(
                                Inset(gfx::Rect(view->layer()->bounds().size()),
                                      view->GetProperty(kPingInsetsKey))
                                    .width() /
                                2.f))),
                        Property(&ui::Layer::type,
                                 Eq(ui::LAYER_SOLID_COLOR)))))),
            Eq(view->layer())));
  }

  // Asserts that a ping exists for `view()` and that the ping is destroyed by
  // invoking the specified `destroy_function`. A ping is considered destroyed
  // for `view()` when its associated ping layers are destroyed. The optionally
  // specified `ping_layers_destroyed_callback` will be run immediately after
  // destruction of all ping layers.
  void AssertPingDestruction(
      base::FunctionRef<void()> destroy_function,
      base::OnceClosure ping_layers_destroyed_callback = base::DoNothing()) {
    // Assert that a ping exists for `view()`.
    ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

    // Observe ping layers.
    NiceMock<MockLayerObserver> layer_observer;
    view()->GetLayersInOrder()[0]->AddObserver(&layer_observer);
    view()->GetLayersInOrder()[0]->children()[0]->AddObserver(&layer_observer);

    // Create a barrier closure to invoke `ping_layers_destroyed_callback` once
    // all ping layers have been destroyed.
    base::RepeatingCallback ping_layer_destroyed_callback =
        base::BarrierClosure(2u, std::move(ping_layers_destroyed_callback));

    // Expect parent ping layer destruction and invoke
    // `ping_layer_destroyed_callback` when that occurs.
    EXPECT_CALL(layer_observer,
                LayerDestroyed(Property(
                    &ui::Layer::name,
                    Eq(UserEducationPingController::kPingParentLayerName))))
        .WillOnce(RunClosure(ping_layer_destroyed_callback));

    // Expect child ping layer destruction and invoke
    // `ping_layer_destroyed_callback` when that occurs.
    EXPECT_CALL(layer_observer,
                LayerDestroyed(Property(
                    &ui::Layer::name,
                    Eq(UserEducationPingController::kPingChildLayerName))))
        .WillOnce(RunClosure(ping_layer_destroyed_callback));

    // Invoked `destroy_function` and expect the ping has been destroyed.
    destroy_function();
    Mock::VerifyAndClearExpectations(&layer_observer);
    ExpectNoPing();
  }

 private:
  // UserEducationPingControllerTest:
  void SetUp() override {
    UserEducationAshTestBase::SetUp();

    // Simulate user login so that a pref service is activated as is needed by
    // the dark/light mode controller.
    SimulateUserLogin("user@test");

    // Create a `widget_` whose contents `view()` will be pinged.
    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(
        views::Builder<views::View>().SetPaintToLayer().Build());
    widget_->CenterWindow(gfx::Size(100, 100));

    // Use a non-zero animation duration so that an interval exists between
    // ping creation and when we are able to check for ping existence since
    // pings are automatically destroyed on animation ended/aborted.
    scoped_animation_duration_scale_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  }

  // Used to enable user education features which are required for existence of
  // the `controller()` under test.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Owns the `view()` to ping.
  std::unique_ptr<views::Widget> widget_;

  // Used to force a non-zero animation duration.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_scale_mode_;
};

// Tests -----------------------------------------------------------------------

// Verifies that pings are destroyed if animations are aborted.
TEST_F(UserEducationPingControllerTest, Abort) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  ASSERT_NO_FATAL_FAILURE(AssertPingDestruction([&]() {
    view()
        ->GetLayersInOrder()[0]
        ->children()[0]
        ->GetAnimator()
        ->StopAnimating();
  }));
}

// Verifies that pings are destroyed if animations are ended.
TEST_F(UserEducationPingControllerTest, End) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  // Note that `ping_animation_ended_future` will wait until all ping layers
  // have been destroyed due to the ping animation having ended.
  base::test::TestFuture<void> ping_animation_ended_future;
  ASSERT_NO_FATAL_FAILURE(AssertPingDestruction(
      [&]() { EXPECT_TRUE(ping_animation_ended_future.Wait()); },
      ping_animation_ended_future.GetCallback()));
}

// Verifies that `UserEducationPingController::GetPingId()` is WAI.
TEST_F(UserEducationPingControllerTest, GetPingId) {
  views::View other_view;

  ExpectNoPing();
  EXPECT_FALSE(controller()->GetPingId(view()));
  EXPECT_FALSE(controller()->GetPingId(&other_view));

  EXPECT_TRUE(CreatePing());
  EXPECT_THAT(controller()->GetPingId(view()), Optional(PingId::kTest1));
  EXPECT_FALSE(controller()->GetPingId(&other_view));

  view()->SetVisible(false);
  EXPECT_FALSE(controller()->GetPingId(view()));
  EXPECT_FALSE(controller()->GetPingId(&other_view));
}

// Verifies that a single view cannot be pinged multiple times concurrently but
// that multiple concurrent pings may exist for distinct views.
TEST_F(UserEducationPingControllerTest, MultipleConcurrent) {
  ExpectNoPing();

  // Ping `view()`.
  EXPECT_TRUE(CreatePing(PingId::kTest1));
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  // Attempts to ping `view()` multiple times concurrently should fail, even if
  // no ping yet exists for the specified ping ID.
  EXPECT_FALSE(CreatePing(PingId::kTest1));
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());
  EXPECT_FALSE(CreatePing(PingId::kTest2));
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  // Add `another_view` to the view hierarchy.
  views::View* another_view = nullptr;
  view()->AddChildView(views::Builder<views::View>()
                           .CopyAddressTo(&another_view)
                           .SetPaintToLayer()
                           .Build());

  // Attempts to ping `another_view` should succeed if and only if no ping yet
  // exists for the specified ping ID.
  EXPECT_FALSE(CreatePing(PingId::kTest1, another_view));
  ExpectNoPing(another_view);
  EXPECT_TRUE(CreatePing(PingId::kTest2, another_view));
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties(another_view));
}

// Verifies that the same `view()` can be pinged multiple times sequentially.
TEST_F(UserEducationPingControllerTest, MultipleSequential) {
  ExpectNoPing();

  {
    // Use a zero duration animation scale mode so that the first ping animation
    // will end instantaneously without requiring for us to wait.
    ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    EXPECT_TRUE(CreatePing());
    ExpectNoPing();
  }

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());
}

// Verifies that a `view()` cannot be pinged if not drawn.
TEST_F(UserEducationPingControllerTest, NotIsDrawn) {
  view()->SetVisible(false);
  ExpectNoPing();

  EXPECT_FALSE(CreatePing());
  ExpectNoPing();
}

// Verifies that pings will update bounds when bounds are updated for the
// associated `view()`.
TEST_F(UserEducationPingControllerTest, OnViewBoundsChanged) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  view()->SetBoundsRect(Inset(view()->bounds(), gfx::Insets(10)));
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());
}

// Verifies that pings are destroyed when the associated `view()` is deleting.
TEST_F(UserEducationPingControllerTest, OnViewIsDeleting) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  // Note that setting a new contents view will result in the destruction of
  // the original contents `view()` associated with the ping.
  ASSERT_NO_FATAL_FAILURE(AssertPingDestruction([&]() {
    view()->GetWidget()->SetContentsView(views::Builder<views::View>().Build());
  }));
}

// Verifies that pings will update bounds when ping insets are updated for the
// associated `view()`.
TEST_F(UserEducationPingControllerTest, OnViewPropertyChanged) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  view()->SetProperty(kPingInsetsKey, gfx::Insets(10));
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());
}

// Verifies that pings will update colors when theme is updated for the
// associated `view()`.
TEST_F(UserEducationPingControllerTest, OnViewThemeChanged) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  auto* const controller = DarkLightModeController::Get();
  controller->SetDarkModeEnabledForTest(!controller->IsDarkModeEnabled());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());
}

// Verifies that pings are destroyed if `view()` stops being visible.
TEST_F(UserEducationPingControllerTest, OnViewVisibilityChanged) {
  ExpectNoPing();

  EXPECT_TRUE(CreatePing());
  ASSERT_NO_FATAL_FAILURE(AssertPingProperties());

  ASSERT_NO_FATAL_FAILURE(
      AssertPingDestruction([&]() { view()->SetVisible(false); }));
}

}  // namespace ash
