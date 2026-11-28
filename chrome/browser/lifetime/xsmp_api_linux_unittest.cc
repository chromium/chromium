// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/xsmp_api_linux.h"

#include <dlfcn.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace xsmp {

TEST(XsmpApiLinuxTest, LoadsAllSymbols) {
  void* ice_lib = dlopen("libICE.so.6", RTLD_LAZY | RTLD_LOCAL);
  void* sm_lib = dlopen("libSM.so.6", RTLD_LAZY | RTLD_LOCAL);
  if (!ice_lib || !sm_lib) {
    EXPECT_EQ(nullptr, XsmpApi::Get());
    GTEST_SKIP() << "libSM.so.6 / libICE.so.6 not available";
  }

  const XsmpApi* api = XsmpApi::Get();
  ASSERT_NE(nullptr, api);
  EXPECT_EQ(api, XsmpApi::Get());
  EXPECT_EQ(reinterpret_cast<void*>(api->connection_number),
            dlsym(ice_lib, "IceConnectionNumber"));
  EXPECT_EQ(reinterpret_cast<void*>(api->process_messages),
            dlsym(ice_lib, "IceProcessMessages"));
  EXPECT_EQ(reinterpret_cast<void*>(api->open_connection),
            dlsym(sm_lib, "SmcOpenConnection"));
  EXPECT_EQ(reinterpret_cast<void*>(api->close_connection),
            dlsym(sm_lib, "SmcCloseConnection"));
  EXPECT_EQ(reinterpret_cast<void*>(api->get_ice_connection),
            dlsym(sm_lib, "SmcGetIceConnection"));
  EXPECT_EQ(reinterpret_cast<void*>(api->save_yourself_done),
            dlsym(sm_lib, "SmcSaveYourselfDone"));
  EXPECT_EQ(reinterpret_cast<void*>(api->set_properties),
            dlsym(sm_lib, "SmcSetProperties"));
}

}  // namespace xsmp
