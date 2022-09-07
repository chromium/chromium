// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_WEB_VIEW_FACTORY_H_
#define ASH_PUBLIC_CPP_ASH_WEB_VIEW_FACTORY_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/ash_web_view.h"

namespace ash {

// A factory implemented in Browser which is responsible for creating instances
// of AshWebView to work around dependency restrictions in Ash.
class ASH_PUBLIC_EXPORT AshWebViewFactory {
 public:
  // Returns the singleton factory instance.
  static AshWebViewFactory* Get();

  // Creates a new AshWebView instance with the given |params|.
  virtual std::unique_ptr<AshWebView> Create(
      const AshWebView::InitParams& params) = 0;

 protected:
  AshWebViewFactory();
  virtual ~AshWebViewFactory();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_WEB_VIEW_FACTORY_H_
