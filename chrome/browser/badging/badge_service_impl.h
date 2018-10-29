// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
#define CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_

#include "base/optional.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/blink/public/platform/modules/badging/badging.mojom.h"

class BadgeServiceImpl : public blink::mojom::BadgeService {
 public:
  BadgeServiceImpl();
  ~BadgeServiceImpl() override;

  static void Create(mojo::InterfaceRequest<BadgeService> request);

  // blink::mojom::BadgeService overrides.
  void SetBadge() override;
  void ClearBadge() override;
};

#endif  // CHROME_BROWSER_BADGING_BADGE_SERVICE_IMPL_H_
