// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_SCREEN_CAPTURE_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_UNIFIED_SCREEN_CAPTURE_TRAY_ITEM_VIEW_H_

#include <map>
#include <string>

#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/containers/fixed_flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace url {
class Origin;
}

namespace ash {

// An indicator shown in UnifiedSystemTray when a web application is using
// screen capturing.
class ASH_EXPORT ScreenCaptureTrayItemView
    : public TrayItemView,
      public MultiCaptureServiceClient::Observer {
  METADATA_HEADER(ScreenCaptureTrayItemView, TrayItemView)

 public:
  struct ScreenCaptureTrayItemMetadata {
    ScreenCaptureTrayItemMetadata();
    explicit ScreenCaptureTrayItemMetadata(base::TimeTicks time_created);
    ScreenCaptureTrayItemMetadata(ScreenCaptureTrayItemMetadata&& metadata);
    ScreenCaptureTrayItemMetadata& operator=(
        ScreenCaptureTrayItemMetadata&& metadata);
    ScreenCaptureTrayItemMetadata(
        const ScreenCaptureTrayItemMetadata& metadata) = delete;
    ScreenCaptureTrayItemMetadata& operator=(
        ScreenCaptureTrayItemMetadata other) = delete;
    virtual ~ScreenCaptureTrayItemMetadata();

    // `time_created` is used to compute for how long the tray item is
    // already shown.
    base::TimeTicks time_created;
    // `closing_timer` is used to make sure that the tray item remains
    // visible for at least six seconds.
    std::unique_ptr<base::OneShotTimer> closing_timer;
  };

  explicit ScreenCaptureTrayItemView(Shelf* shelf);
  ScreenCaptureTrayItemView(const ScreenCaptureTrayItemView&) = delete;
  ScreenCaptureTrayItemView& operator=(const ScreenCaptureTrayItemView&) =
      delete;
  ~ScreenCaptureTrayItemView() override;

  // views::View:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& point) const override;

  // TrayItemView:
  void HandleLocaleChange() override {}
  void UpdateLabelOrImageViewColor(bool active) override;

  // MultiCaptureServiceClient::Observer:
  void MultiCaptureStarted(const std::string& label,
                           const url::Origin& origin) override;
  void MultiCaptureStartedFromApp(const std::string& label,
                                  const std::string& app_id,
                                  const std::string& app_short_name) override;
  void MultiCaptureStopped(const std::string& label) override;
  void MultiCaptureServiceClientDestroyed() override;

 protected:
  virtual void Refresh();

 private:
  friend class ScreenCaptureTrayItemViewTest;
  FRIEND_TEST_ALL_PREFIXES(ScreenCaptureTrayItemViewTest,
                           SingleOriginCaptureStartedAndStopped);
  FRIEND_TEST_ALL_PREFIXES(ScreenCaptureTrayItemViewTest,
                           MultiOriginCaptureStartedAndStopped);
  FRIEND_TEST_ALL_PREFIXES(
      ScreenCaptureTrayItemViewTest,
      MultiOriginCaptureStartedAndEarlyStoppedExpectedDelayedStoppedCallback);

  std::map<std::string, ScreenCaptureTrayItemMetadata> requests_;

  base::ScopedObservation<MultiCaptureServiceClient,
                          MultiCaptureServiceClient::Observer>
      multi_capture_service_client_observation_{this};

  base::WeakPtrFactory<ScreenCaptureTrayItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_SCREEN_CAPTURE_TRAY_ITEM_VIEW_H_
