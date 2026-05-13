// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/local_printer_ash.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/test_local_printer_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/ipp_client_info_calculator.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/oauth2/mock_authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#else
#include "base/notreached.h"
#endif

using ::chromeos::Printer;
using ::chromeos::PrinterClass;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

namespace printing {


TEST(LocalPrinterAsh, ConfigToMojom) {
  ash::PrintServersConfig config;
  config.fetching_mode = crosapi::mojom::PrintServersConfig::
      ServerPrintersFetchingMode::kSingleServerOnly;
  config.print_servers.emplace_back("id", GURL("http://localhost"), "name");
  crosapi::mojom::PrintServersConfigPtr mojom =
      crosapi::LocalPrinterAsh::ConfigToMojom(config);
  ASSERT_TRUE(mojom);
  EXPECT_EQ(crosapi::mojom::PrintServersConfig::ServerPrintersFetchingMode::
                kSingleServerOnly,
            mojom->fetching_mode);
  ASSERT_EQ(1u, mojom->print_servers.size());
  EXPECT_EQ("id", mojom->print_servers[0]->id);
  EXPECT_EQ(GURL("http://localhost"), mojom->print_servers[0]->url);
  EXPECT_EQ("name", mojom->print_servers[0]->name);
}


}  // namespace printing
