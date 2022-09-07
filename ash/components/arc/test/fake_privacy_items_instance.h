// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_PRIVACY_ITEMS_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_PRIVACY_ITEMS_INSTANCE_H_

#include "ash/components/arc/mojom/privacy_items.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"

namespace arc {

class FakePrivacyItemsInstance : public mojom::PrivacyItemsInstance {
 public:
  FakePrivacyItemsInstance();

  FakePrivacyItemsInstance(const FakePrivacyItemsInstance&) = delete;
  FakePrivacyItemsInstance& operator=(const FakePrivacyItemsInstance&) = delete;

  ~FakePrivacyItemsInstance() override;

  int last_bounds_display_id() const { return last_bounds_display_id_; }
  std::vector<gfx::Rect> last_bounds() const { return last_bounds_; }

  // mojom::PrivacyItemsInstance overrides:
  void Init(mojo::PendingRemote<mojom::PrivacyItemsHost> host_remote,
            InitCallback callback) override;
  void OnStaticPrivacyIndicatorBoundsChanged(
      int32_t display_id,
      const std::vector<gfx::Rect>& bounds) override;

 private:
  mojo::Remote<mojom::PrivacyItemsHost> host_remote_;
  int32_t last_bounds_display_id_ = display::kInvalidDisplayId;
  std::vector<gfx::Rect> last_bounds_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_PRIVACY_ITEMS_INSTANCE_H_
