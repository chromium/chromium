// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/language_detection/public/cpp/language_detection_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class LanguageDetectionServiceTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(LanguageDetectionServiceTest,
                       DetermineLanguageReliable) {
  mojo::Remote<language_detection::mojom::LanguageDetectionService> service =
      language_detection::LaunchLanguageDetectionService();
  base::string16 text = base::UTF8ToUTF16(
      "El niño atrapó un dorado muy grande con cebo vivo. Fileteó el "
      "pescado y lo asó a la parrilla. Sabía excelente. Espera pescar otro "
      "buen pescado mañana.");

  base::RunLoop run_loop;
  service->DetermineLanguage(
      text, base::BindLambdaForTesting(
                [&](const std::string& language, bool is_reliable) {
                  EXPECT_EQ("es", language);
                  EXPECT_TRUE(is_reliable);
                  run_loop.Quit();
                }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionServiceTest,
                       DetermineLanguageUndeterminedUnreliable) {
  mojo::Remote<language_detection::mojom::LanguageDetectionService> service =
      language_detection::LaunchLanguageDetectionService();
  base::string16 text = base::UTF8ToUTF16("Not enough text for detection");

  base::RunLoop run_loop;
  service->DetermineLanguage(
      text, base::BindLambdaForTesting(
                [&](const std::string& language, bool is_reliable) {
                  EXPECT_EQ("und", language);
                  EXPECT_FALSE(is_reliable);
                  run_loop.Quit();
                }));
  run_loop.Run();
}
