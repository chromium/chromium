// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/logo_view/logo_view.h"

#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "ash/assistant/ui/logo_view/logo_view_impl.h"
#endif

namespace ash {

LogoView::LogoView() = default;

LogoView::~LogoView() = default;

// static
LogoView* LogoView::Create() {
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  return new LogoViewImpl();
#else
  return new LogoView();
#endif
}

}  // namespace ash
