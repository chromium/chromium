// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mixin_based_extension_apitest.h"

#include "chrome/browser/extensions/extension_apitest.h"

template class InProcessBrowserTestMixinHostSupport<
    extensions::ExtensionApiTest>;
