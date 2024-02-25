// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_PROVIDER_H_

#include <memory>

#include "ash/public/cpp/ash_web_view.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"

#include "ash/webui/eche_app_ui/proto/accessibility_mojom.pb.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"

namespace ash::eche_app {

class AccessibilityProviderProxy {
 public:
  virtual ~AccessibilityProviderProxy() = default;

  virtual bool UseFullFocusMode() = 0;
  virtual bool IsAccessibilityEnabled() = 0;
  virtual ax::android::mojom::AccessibilityFilterType GetFilterType() = 0;
  virtual void OnViewTracked() = 0;
  virtual void SetAccessibilityEnabledStateChangedCallback(
      base::RepeatingCallback<void(bool)>) = 0;
  virtual void SetExploreByTouchEnabledStateChangedCallback(
      base::RepeatingCallback<void(bool)>) = 0;
};
class AccessibilityProvider
    : public mojom::AccessibilityProvider,
      public ax::android::AXTreeSourceAndroid::Delegate {
 public:
  explicit AccessibilityProvider(
      std::unique_ptr<AccessibilityProviderProxy> proxy);
  ~AccessibilityProvider() override;

  // Track the current eche web view.
  void TrackView(AshWebView* view);
  void HandleStreamClosed();
  // Handles the result of a refreshWithExtraData call.
  void OnGetTextLocationDataResult(const ui::AXActionData& action,
                                   const std::optional<std::vector<uint8_t>>&
                                       serialized_text_location) const;

  // mojom::AccessibilityProvider overrides.
  // Proto from ash/webui/eche_app_ui/proto/accessibility_mojom.proto.
  void HandleAccessibilityEventReceived(
      const std::vector<uint8_t>& serialized_proto) override;
  void SetAccessibilityObserver(
      ::mojo::PendingRemote<mojom::AccessibilityObserver> observer) override;
  void IsAccessibilityEnabled(IsAccessibilityEnabledCallback callback) override;

  void Bind(mojo::PendingReceiver<mojom::AccessibilityProvider> receiver);

  // AXTreeSourceAndroid::Delegate
  void OnAction(const ui::AXActionData& data) const override;
  bool UseFullFocusMode() const override;

 private:
  ax::android::mojom::AccessibilityFilterType GetFilterType();
  void UpdateDeviceBounds(const proto::Rect& device_bounds);
  void HandleHitTest(const ui::AXActionData& data) const;
  gfx::Rect OnGetTextLocationDataResultInternal(proto::Rect proto_rect) const;
  // Handles the result from perform action.
  void OnActionResult(const ui::AXActionData& data, bool result) const;
  void OnAccessibilityEnabledStateChanged(bool enabled);
  void OnExploreByTouchEnabledStateChanged(bool enabled);

  class SerializationDelegate
      : public ax::android::AXTreeSourceAndroid::SerializationDelegate {
   public:
    explicit SerializationDelegate(gfx::Rect& device_bounds);

    void PopulateBounds(const ax::android::AccessibilityInfoDataWrapper& node,
                        ui::AXNodeData& out_data) const override;

   private:
    gfx::RectF ScaleAndroidPxToChromePx(
        const ax::android::AccessibilityInfoDataWrapper& node,
        aura::Window* window) const;
    const raw_ref<gfx::Rect> device_bounds_;
  };

  mojo::Receiver<mojom::AccessibilityProvider> receiver_{this};
  mojo::Remote<mojom::AccessibilityObserver> observer_remote_;
  // Eche can only have one app visible at a time, so we only need one tree
  // source. In the future we can swap this to a map if more windows are added.
  std::unique_ptr<ax::android::AXTreeSourceAndroid> tree_source_;

  bool use_full_focus_mode_ = false;

  // device settings
  gfx::Rect device_bounds_;

  // Proxy for accessing accessibility manager in chrome/
  std::unique_ptr<AccessibilityProviderProxy> proxy_;
  base::WeakPtrFactory<AccessibilityProvider> weak_ptr_factory_{this};
};
}  // namespace ash::eche_app
#endif  // ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_PROVIDER_H_
