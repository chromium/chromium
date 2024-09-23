// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/skia_gold_pixel_diff.h"

namespace qrcode_generator {
namespace {

class QrCodeGeneratorServicePixelTest : public PlatformBrowserTest {
 public:
  QrCodeGeneratorServicePixelTest() = default;

  void TestGolden(const std::string& data,
                  const qr_code_generator::CenterImage& center_image,
                  const qr_code_generator::ModuleStyle& module_style,
                  const qr_code_generator::LocatorStyle& locator_style) {
    base::HistogramTester histograms;
    auto response = qr_code_generator::GenerateBitmap(
        base::as_byte_span(data), module_style, locator_style, center_image,
        qr_code_generator::QuietZone::kIncluded);

    // Verify that we got a successful response.
    ASSERT_TRUE(response.has_value());

    // Version 1 of QR codes has 21x21 modules/tiles/pixels.  Verify that the
    // returned QR image has a size that is at least 21x21.
    ASSERT_GE(response->height(), 21);
    ASSERT_GE(response->width(), 21);

    // The QR code should be a square.
    ASSERT_EQ(response->width(), response->height());

    // Verify that the expected UMA metrics got logged.
    // TODO(crbug.com/40789042): Cover BytesToQrPixels and QrPixelsToQrImage as
    // well.
    histograms.ExpectTotalCount("Sharing.QRCodeGeneration.Duration", 1);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
    // Verify image contents through go/chrome-engprod-skia-gold.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kVerifyPixels)) {
      const ::testing::TestInfo* test_info =
          ::testing::UnitTest::GetInstance()->current_test_info();
      ui::test::SkiaGoldPixelDiff* pixel_diff =
          ui::test::SkiaGoldPixelDiff::GetSession();
      ASSERT_TRUE(pixel_diff);
      ASSERT_TRUE(pixel_diff->CompareScreenshot(
          ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
              test_info, ui::test::SkiaGoldPixelDiff::GetPlatform()),
          response.value()));
    }
#endif
  }
};

IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest,
                       DinoWithRoundQrPixelsAndLocators) {
  TestGolden("https://example.com", qr_code_generator::CenterImage::kDino,
             qr_code_generator::ModuleStyle::kCircles,
             qr_code_generator::LocatorStyle::kRounded);
}

IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest,
                       PassKeyWithSquareQrPixelsAndLocators) {
  TestGolden("https://example.com", qr_code_generator::CenterImage::kPasskey,
             qr_code_generator::ModuleStyle::kSquares,
             qr_code_generator::LocatorStyle::kSquare);
}

IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest,
                       ProductLogoWithSquareQrPixelsAndLocators) {
  TestGolden("https://example.com",
             qr_code_generator::CenterImage::kProductLogo,
             qr_code_generator::ModuleStyle::kSquares,
             qr_code_generator::LocatorStyle::kSquare);
}

// This is a regression test for https://crbug.com/1334066.  It tests that the
// QR code generator can handle fairly big inputs (the URL below is more than
// 800 bytes long).
//
// The pixel test verifies that the output of the QR code generator doesn't
// change.  The real verification is checking if the generated QR code can be
// used by a variety of QR code readers (e.g. by Chrome, Safari, etc.) to
// navigate to the https://md5calc.com/hash/crc32/... URL - the result of such
// navigation should show a HTML page that says: CRC32 hash is "9f329afa".
//
// Note that https://www.qrcode.com/en/howto/code.html points out that there
// should be light/white space of 4 or more modules/pixels around the QR code
// but currently that margin is added by higher layers of code.  And therefore
// the verification should be done after embedding or printing the generated
// image onto a light background.
//
// Limits for input sizes at version 40 of QR codes can be found at
// https://www.qrcode.com/en/about/version.html - 2331 is the limit for
// an input containing arbitrary bytes.  In practice though QR codes that are
// so big may not be recognized by QR code readers and therefore the test uses
// a slightly smaller input.
IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest, BigUrl) {
  const char kInput[] =
      "https://md5calc.com/hash/crc32/"
      "Lorem+ipsum+dolor+sit+amet,+consectetur+adipiscing+elit.+Donec+sit+amet+"
      "odio+sit+amet+arcu+lacinia+imperdiet+eget+vitae+ante.+Integer+a+urna+ac+"
      "ipsum+vehicula+consequat.+Quisque+vel+maximus+dolor.+Donec+ullamcorper+"
      "lectus+at+augue+imperdiet,+vel+lacinia+lacus+euismod.+Proin+vestibulum+"
      "eget+ipsum+eu+laoreet.+Vivamus+commodo+malesuada+ipsum+sit+amet+mollis.+"
      "Praesent+et+facilisis+sem.++Nulla+sit+amet+dolor+id+lectus+mattis+"
      "laoreet.+Sed+arcu+dolor,+sodales+vel+nisl+in,+convallis+elementum+"
      "sapien.+Pellentesque+vestibulum+neque+et+nisl+ultrices,+vel+congue+"
      "sapien+bibendum.+Aliquam+ornare+in+ante+ac+dignissim.+Interdum+et+"
      "malesuada+fames+ac+ante+ipsum+primis+in+faucibus.+Sed+magna+tortor,+"
      "ornare+ac+bibendum+ac,+ultricies+nec+nisl.+Maecenas+consequat+interdum+"
      "ipsum+a+ultrices.";
  TestGolden(kInput, qr_code_generator::CenterImage::kDino,
             qr_code_generator::ModuleStyle::kCircles,
             qr_code_generator::LocatorStyle::kRounded);
}

// This is a regression test for https://crbug.com/1334066.  It tests that the
// QR code generator can handle fairly big inputs (the URL below is 2331 bytes
// long).
//
// The pixel test verifies that the output of the QR code generator doesn't
// change.  The real verification is checking if the generated QR code can be
// used by a variety of QR code readers (e.g. by Chrome, Safari, etc.) to
// navigate to the https://md5calc.com/hash/crc32/... URL - the result of such
// navigation should show a HTML page that says: CRC32 hash is "b6e1c7ad".
// OTOH, a QR code this bug may not be recognized by all QR code readers, so
// it's okay if the "real verification" is skipped for *this* particular test
// (`BigUrl` should still work).
//
// Note that https://www.qrcode.com/en/howto/code.html points out that there
// should be light/white space of 4 or more modules/pixels around the QR code
// but currently that margin is added by higher layers of code.  And therefore
// the verification should be done after embedding or printing the generated
// image onto a light background.
//
// Limits for input sizes at version 40 of QR codes can be found at
// https://www.qrcode.com/en/about/version.html - 2331 is indeed the limit for
// an input containing arbitrary bytes.  In theory, a smart segmentation
// algorithm could support longer URLs, but only if the input consistent of less
// arbitrary bytes, and contained more digits and/or UPPER case alphabetic
// characters.
IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest, HugeUrl) {
  const char kInput[] =
      "https://md5calc.com/hash/crc32/"
      "Lorem+ipsum+dolor+sit+amet,+consectetur+adipiscing+elit.+Donec+sit+amet+"
      "odio+sit+amet+arcu+lacinia+imperdiet+eget+vitae+ante.+Integer+a+urna+ac+"
      "ipsum+vehicula+consequat.+Quisque+vel+maximus+dolor.+Donec+ullamcorper+"
      "lectus+at+augue+imperdiet,+vel+lacinia+lacus+euismod.+Proin+vestibulum+"
      "eget+ipsum+eu+laoreet.+Vivamus+commodo+malesuada+ipsum+sit+amet+mollis.+"
      "Praesent+et+facilisis+sem.++Nulla+sit+amet+dolor+id+lectus+mattis+"
      "laoreet.+Sed+arcu+dolor,+sodales+vel+nisl+in,+convallis+elementum+"
      "sapien.+Pellentesque+vestibulum+neque+et+nisl+ultrices,+vel+congue+"
      "sapien+bibendum.+Aliquam+ornare+in+ante+ac+dignissim.+Interdum+et+"
      "malesuada+fames+ac+ante+ipsum+primis+in+faucibus.+Sed+magna+tortor,+"
      "ornare+ac+bibendum+ac,+ultricies+nec+nisl.+Maecenas+consequat+interdum+"
      "ipsum+a+ultrices.+Nam+sit+amet+mollis+neque.++Morbi+iaculis+justo+quis+"
      "ipsum+condimentum+semper.+Vestibulum+a+eleifend+enim.+Aenean+in+elit+et+"
      "arcu+ultrices+auctor.+In+tempus+elit+ac+auctor+pellentesque.+Donec+"
      "semper+sapien+eu+augue+vestibulum,+ac+facilisis+nunc+sodales.+Ut+"
      "facilisis,+nisl+a+gravida+ullamcorper,+mi+felis+viverra+ligula,+eu+"
      "commodo+justo+arcu+eget+erat.+Maecenas+id+iaculis+nisi,+non+sagittis+"
      "urna.+Vivamus+eget+condimentum+ex,+vel+fringilla+ex.++Etiam+porttitor+"
      "facilisis+tellus+quis+aliquam.+In+vitae+elit+quis+orci+porta+placerat.+"
      "Proin+laoreet+feugiat+ipsum,+non+commodo+nisi+mollis+molestie.+Nunc+"
      "auctor+ante+sed+nisl+tincidunt,+vitae+mattis+urna+auctor.+Fusce+iaculis+"
      "laoreet+odio+ac+interdum.+Sed+gravida+dui+diam,+non+blandit+velit+"
      "auctor+sit+amet.+Etiam+a+dolor+eu+lorem+porttitor+molestie.+Praesent+"
      "mattis+varius+velit+a+tempus.+Etiam+sit+amet+mollis+turpis.+Donec+porta+"
      "lectus+urna,+sagittis+fringilla+nulla+tincidunt+nec.+Curabitur+"
      "facilisis,+lectus+ut+vulputate+posuere,+magna+ante+fermentum+est,+in+"
      "imperdiet+neque+nisl+facilisis+eros.+Quisque+ut+odio+eget+orci+cursus+"
      "semper+et+sit+amet+augue.+Nam+nec+nunc+pharetra,+rhoncus+purus+mollis,+"
      "posuere+metus.++Sed+vestibulum+nisl+eget+iaculis+ullamcorper.+Quisque+"
      "quis+nibh+imperdiet,+eleifend+erat+non,+pulvinar+dolor.+Pellentesque+"
      "felis+est,+sollicitudin+a+ipsum+nec,+lacinia+pharetra+metus.+Morbi+"
      "neque+leo,+sodales+ac+viverra+in,+sollicitudin+non+est.+Aenean+"
      "dignissim+quam+quis+nibh+tempus+rhoncus.+Quisque+in+sapien+vitae+lectus+"
      "malesuada+finibus+et+et+n";
  TestGolden(kInput, qr_code_generator::CenterImage::kDino,
             qr_code_generator::ModuleStyle::kCircles,
             qr_code_generator::LocatorStyle::kRounded);
}

}  // namespace
}  // namespace qrcode_generator
