// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_H_
#define ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"

namespace ash {

class ProgressIconAnimation;
class ProgressIndicatorAnimationRegistry;
class ProgressRingAnimation;

// A class owning a `ui::Layer` which paints indication of progress.
// NOTE: The owned `layer()` is not painted if progress == `1.f`.
class ASH_EXPORT ProgressIndicator : public ui::LayerOwner,
                                     public ui::LayerDelegate {
 public:
  static constexpr char kClassName[] = "ProgressIndicator";
  static constexpr float kProgressComplete = 1.f;

  ProgressIndicator(const ProgressIndicator&) = delete;
  ProgressIndicator& operator=(const ProgressIndicator&) = delete;
  ~ProgressIndicator() override;

  // Returns an instance which paints indication of progress returned by the
  // specified `progress_callback`. NOTE: This instance comes pre-wired with an
  // `animation_registry_` that will manage progress animations as needed.
  static std::unique_ptr<ProgressIndicator> CreateDefaultInstance(
      base::RepeatingCallback<absl::optional<float>()> progress_callback);

  // Adds the specified `callback` to be notified of `progress_` changes. The
  // `callback` will continue to receive events so long as both `this` and the
  // returned subscription exist.
  base::CallbackListSubscription AddProgressChangedCallback(
      base::RepeatingClosureList::CallbackType callback);

  // Creates and returns the `layer()` which is owned by this progress
  // indicator. Note that this may only be called if `layer()` does not exist.
  using ColorResolver = base::RepeatingCallback<SkColor(ui::ColorId)>;
  ui::Layer* CreateLayer(ColorResolver color_resolver);

  // Destroys the `layer()` which is owned by this progress indicator. Note that
  // this will no-op if `layer()` does not exist.
  void DestroyLayer();

  // Invoke to schedule repaint of the entire `layer()`.
  void InvalidateLayer();

  // Sets the `color_id` to use in lieu of the default when painting progress
  // indication. If `color_id` is absent, default colors are used.
  void SetColorId(const absl::optional<ui::ColorId>& color_id);

  // Sets the visibility for this progress indicator's inner icon. Note that
  // the inner icon will only be painted while `progress_` is incomplete,
  // regardless of the value of `visible` provided.
  void SetInnerIconVisible(bool visible);
  bool inner_icon_visible() const { return inner_icon_visible_; }

  // Returns the underlying `animation_registry_` in which to look up animations
  // for the associated `animation_key_`. NOTE: This may return `nullptr`.
  ProgressIndicatorAnimationRegistry* animation_registry() {
    return animation_registry_;
  }

  // Returns the `animation_key_` for which to look up animations in the
  // underlying `animation_registry_`. NOTE: This may return `nullptr`.
  const void* animation_key() const { return animation_key_; }

  // Returns the underlying `progress_` for which to paint indication.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  const absl::optional<float>& progress() const { return progress_; }

 protected:
  // Each progress indicator is associated with an `animation_key_` which is
  // used to look up animations in the provided `animation_registry`. When an
  // animation exists, it will be painted in lieu of the determinate progress
  // indication that would otherwise be painted for the cached `progress_`.
  // NOTE: `animation_registry` may be `nullptr` if animations are not needed.
  ProgressIndicator(ProgressIndicatorAnimationRegistry* animation_registry,
                    const void* animation_key);

  // Returns the calculated progress to paint to the owned `layer()`. This is
  // invoked during `UpdateVisualState()` just prior to painting.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  // NOTE: If progress == `1.f`, progress is complete and will not be painted.
  virtual absl::optional<float> CalculateProgress() const = 0;

 private:
  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_scale, float new_scale) override;
  void OnPaintLayer(const ui::PaintContext& context) override;
  void UpdateVisualState() override;

  // Invoked when the icon `animation` associated with this progress indicator's
  // `animation_key_` has changed in the `animation_registry_`.
  // NOTE: The specified `animation` may be `nullptr`.
  void OnProgressIconAnimationChanged(ProgressIconAnimation* animation);

  // Invoked when the ring `animation` associated with this progress indicator's
  // `animation_key_` has changed in the `animation_registry_`.
  // NOTE: The specified `animation` may be `nullptr`.
  void OnProgressRingAnimationChanged(ProgressRingAnimation* animation);

  // The animation registry in which to look up animations for the associated
  // `animation_key_`. When an animation exists, it will be painted in lieu of
  // the determinate progress indication that would otherwise be painted for the
  // cached `progress_`.
  const raw_ptr<ProgressIndicatorAnimationRegistry,
                DanglingUntriaged | ExperimentalAsh>
      animation_registry_;

  // The key for which to look up animations in the `animation_registry_`.
  // When an animation exists, it will be painted in lieu of the determinate
  // progress indication that would otherwise be painted for the cached
  // `progress_`.
  const raw_ptr<const void, ExperimentalAsh> animation_key_;

  // A subscription to receive events when the icon animation associated with
  // this progress indicator's `animation_key_` has changed in the
  // `animation_registry_`.
  base::CallbackListSubscription icon_animation_changed_subscription_;

  // A subscription to receive events on updates to the icon animation owned by
  // the `animation_registry_` which is associated with this progress
  // indicator's `animation_key_`. On icon animation update, the progress
  // indicator will `InvalidateLayer()` to trigger paint of the next animation
  // frame.
  base::CallbackListSubscription icon_animation_updated_subscription_;

  // A subscription to receive events when the ring animation associated with
  // this progress indicator's `animation_key_` has changed in the
  // `animation_registry_`.
  base::CallbackListSubscription ring_animation_changed_subscription_;

  // A subscription to receive events on updates to the ring animation owned by
  // the `animation_registry_` which is associated with this progress
  // indicator's `animation_key_`. On ring animation update, the progress
  // indicator will `InvalidateLayer()` to trigger paint of the next animation
  // frame.
  base::CallbackListSubscription ring_animation_updated_subscription_;

  // Used to resolve the color to use to paint progress indication. Non-null if
  // and only if the `layer()` which is owned by this progress indicator exists.
  ColorResolver color_resolver_;

  // The color ID to use in lieu of the default when painting progress
  // indication. If absent, default colors are used.
  absl::optional<ui::ColorId> color_id_;

  // Cached progress returned from `CalculateProgress()` just prior to painting.
  // NOTE: If absent, progress is indeterminate.
  // NOTE: If present, progress must be >= `0.f` and <= `1.f`.
  absl::optional<float> progress_;

  // The list of callbacks for which to notify `progress_` changes.
  base::RepeatingClosureList progress_changed_callback_list_;

  // Whether this progress indicator's inner icon is visible. Note that the
  // inner icon will only be painted while `progress_` is incomplete, regardless
  // of this value.
  bool inner_icon_visible_ = true;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_H_
