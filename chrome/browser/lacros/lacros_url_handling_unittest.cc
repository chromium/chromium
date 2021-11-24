// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_url_handling.h"

#include "base/test/task_environment.h"
#include "chrome/common/url_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(LacrosUrlHandlingTest, IsURLAcceptedByAshOldVersion) {
  base::test::TaskEnvironment task_environment;
  chromeos::LacrosService lacros_service;

  auto params = crosapi::mojom::BrowserInitParams::New();
  lacros_service.SetInitParamsForTests(std::move(params));
  EXPECT_TRUE(lacros_url_handling::IsUrlAcceptedByAsh(
      GURL(chrome::kChromeUIOSSettingsURL)));
  EXPECT_TRUE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL(chrome::kChromeUIFlagsURL)));
  EXPECT_FALSE(lacros_url_handling::IsUrlAcceptedByAsh(GURL("")));
  EXPECT_FALSE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://flags2")));
}

TEST(LacrosUrlHandlingTest, IsURLAcceptedByAsh) {
  base::test::TaskEnvironment task_environment;
  chromeos::LacrosService lacros_service;

  auto params = crosapi::mojom::BrowserInitParams::New();
  params->accepted_internal_ash_urls = std::vector<GURL>{
      GURL(chrome::kChromeUIFlagsURL), GURL(chrome::kChromeUIOSSettingsURL),
      GURL("chrome://version"), GURL("chrome://settings/network")};
  lacros_service.SetInitParamsForTests(std::move(params));
  EXPECT_TRUE(lacros_url_handling::IsUrlAcceptedByAsh(
      GURL(chrome::kChromeUIOSSettingsURL)));
  EXPECT_TRUE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL(chrome::kChromeUIFlagsURL)));
  EXPECT_TRUE(lacros_url_handling::IsUrlAcceptedByAsh(
      GURL("chrome://settings/network")));
  EXPECT_TRUE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://version")));
  EXPECT_FALSE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://versions")));
  EXPECT_FALSE(lacros_url_handling::IsUrlAcceptedByAsh(GURL("http://version")));
  EXPECT_FALSE(lacros_url_handling::IsUrlAcceptedByAsh(GURL("")));
  EXPECT_FALSE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://flags2")));
}
