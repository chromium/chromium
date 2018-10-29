// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/sdk_forward_declarations.h"

#if !defined(MAC_OS_X_VERSION_10_10) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10
NSString* const CIDetectorTypeQRCode = @"CIDetectorTypeQRCode";

NSString* const NSUserActivityTypeBrowsingWeb =
    @"NSUserActivityTypeBrowsingWeb";

NSString* const NSAppearanceNameVibrantDark = @"NSAppearanceNameVibrantDark";
#endif  // MAC_OS_X_VERSION_10_10

#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11
NSString* const CIDetectorTypeText = @"CIDetectorTypeText";
#endif  // MAC_OS_X_VERSION_10_11

#if !defined(MAC_OS_X_VERSION_10_14) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_14
NSString* const NSAppearanceNameDarkAqua = @"NSAppearanceNameDarkAqua";
#endif
