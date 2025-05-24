// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_INTERNAL_TEST_HEADERS_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_INTERNAL_TEST_HEADERS_H_

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

// For glic developers, if you need to change a header path in this file, please
// check the actual header usage in the internal repo and update there if
// needed. Link to the internal test repo: http://shortn/_ec8KxkHMBV

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_INTERNAL_TEST_HEADERS_H_
