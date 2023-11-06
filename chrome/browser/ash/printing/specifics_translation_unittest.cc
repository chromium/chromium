// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ash/printing/specifics_translation.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync/protocol/printer_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::chromeos::Printer;

constexpr char kId[] = "UNIQUE_ID";
constexpr char kDisplayName[] = "Best Printer Ever";
constexpr char kDescription[] = "The green one";
constexpr char kManufacturer[] = "Manufacturer";
constexpr char kModel[] = "MODEL";
constexpr char kMakeAndModel[] = "Manufacturer MODEL";
constexpr char kUri[] = "ipps://notaprinter.chromium.org:123/ipp/print";
constexpr char kUuid[] = "UUIDUUIDUUID";
const base::Time kUpdateTime = base::Time::FromInternalValue(22114455660000);

constexpr char kUserSuppliedPPD[] = "file://foo/bar/baz/eeaaaffccdd00";
constexpr char kEffectiveMakeAndModel[] = "Manufacturer Model T1000";

}  // namespace

namespace ash {

TEST(SpecificsTranslationTest, SpecificsToPrinter) {
  sync_pb::PrinterSpecifics specifics;
  specifics.set_id(kId);
  specifics.set_display_name(kDisplayName);
  specifics.set_description(kDescription);
  specifics.set_make_and_model(kMakeAndModel);
  specifics.set_uri(kUri);
  specifics.set_uuid(kUuid);
  specifics.set_updated_timestamp(kUpdateTime.InMillisecondsSinceUnixEpoch());

  sync_pb::PrinterPPDReference ppd;
  ppd.set_effective_make_and_model(kEffectiveMakeAndModel);
  *specifics.mutable_ppd_reference() = ppd;

  std::unique_ptr<Printer> result = SpecificsToPrinter(specifics);
  EXPECT_EQ(kId, result->id());
  EXPECT_EQ(kDisplayName, result->display_name());
  EXPECT_EQ(kDescription, result->description());
  EXPECT_EQ(kMakeAndModel, result->make_and_model());
  EXPECT_EQ(kUri, result->uri().GetNormalized(false));
  EXPECT_EQ(kUuid, result->uuid());

  EXPECT_EQ(kEffectiveMakeAndModel,
            result->ppd_reference().effective_make_and_model);
  EXPECT_FALSE(result->IsIppEverywhere());
}

TEST(SpecificsTranslationTest, SpecificsToPrinterSocketUriWithPath) {
  sync_pb::PrinterSpecifics specifics;
  specifics.set_id(kId);
  specifics.set_display_name(kDisplayName);
  specifics.set_description(kDescription);
  specifics.set_make_and_model(kMakeAndModel);
  specifics.set_uri("socket://abc.def:1234/path1/path2");
  specifics.set_uuid(kUuid);
  specifics.set_updated_timestamp(kUpdateTime.InMillisecondsSinceUnixEpoch());

  sync_pb::PrinterPPDReference ppd;
  ppd.set_effective_make_and_model(kEffectiveMakeAndModel);
  *specifics.mutable_ppd_reference() = ppd;

  std::unique_ptr<Printer> result = SpecificsToPrinter(specifics);
  EXPECT_EQ("socket://abc.def:1234", result->uri().GetNormalized());
}

TEST(SpecificsTranslationTest, PrinterToSpecifics) {
  Printer printer;
  printer.set_id(kId);
  printer.set_display_name(kDisplayName);
  printer.set_description(kDescription);
  printer.set_make_and_model(kMakeAndModel);
  printer.SetUri(kUri);
  printer.set_uuid(kUuid);

  Printer::PpdReference ppd;
  ppd.effective_make_and_model = kEffectiveMakeAndModel;
  *printer.mutable_ppd_reference() = ppd;

  std::unique_ptr<sync_pb::PrinterSpecifics> result =
      PrinterToSpecifics(printer);
  EXPECT_EQ(kId, result->id());
  EXPECT_EQ(kDisplayName, result->display_name());
  EXPECT_EQ(kDescription, result->description());
  EXPECT_EQ(kMakeAndModel, result->make_and_model());
  EXPECT_EQ(kUri, result->uri());
  EXPECT_EQ(kUuid, result->uuid());

  EXPECT_EQ(kEffectiveMakeAndModel,
            result->ppd_reference().effective_make_and_model());
}

TEST(SpecificsTranslationTest, SpecificsToPrinterRoundTrip) {
  Printer printer;
  printer.set_id(kId);
  printer.set_display_name(kDisplayName);
  printer.set_description(kDescription);
  printer.set_make_and_model(kMakeAndModel);
  printer.SetUri(kUri);
  printer.set_uuid(kUuid);

  Printer::PpdReference ppd;
  ppd.autoconf = true;
  *printer.mutable_ppd_reference() = ppd;

  std::unique_ptr<sync_pb::PrinterSpecifics> temp = PrinterToSpecifics(printer);
  std::unique_ptr<Printer> result = SpecificsToPrinter(*temp);

  EXPECT_EQ(kId, result->id());
  EXPECT_EQ(kDisplayName, result->display_name());
  EXPECT_EQ(kDescription, result->description());
  EXPECT_EQ(kMakeAndModel, result->make_and_model());
  EXPECT_EQ(kUri, result->uri().GetNormalized(false));
  EXPECT_EQ(kUuid, result->uuid());

  EXPECT_TRUE(result->ppd_reference().effective_make_and_model.empty());
  EXPECT_TRUE(result->ppd_reference().autoconf);
}

TEST(SpecificsTranslationTest, MergePrinterToSpecifics) {
  sync_pb::PrinterSpecifics original;
  original.set_id(kId);
  original.mutable_ppd_reference()->set_autoconf(true);
  original.set_manufacturer(kManufacturer);
  original.set_model(kModel);
  // make_and_model not set

  Printer printer(kId);
  printer.mutable_ppd_reference()->effective_make_and_model =
      kEffectiveMakeAndModel;
  printer.set_make_and_model(kMakeAndModel);
  // manufacturer not set
  // model not set

  MergePrinterToSpecifics(printer, &original);

  EXPECT_EQ(kId, original.id());
  EXPECT_EQ(kEffectiveMakeAndModel,
            original.ppd_reference().effective_make_and_model());

  // Verify that autoconf is cleared.
  EXPECT_FALSE(original.ppd_reference().autoconf());

  // Verify that both make_and_model and the old fields are retained.
  EXPECT_EQ(kMakeAndModel, original.make_and_model());
  EXPECT_EQ(kManufacturer, original.manufacturer());
  EXPECT_EQ(kModel, original.model());
}

// Tests that the autoconf value overrides other PpdReference fields.
TEST(SpecificsTranslationTest, AutoconfOverrides) {
  sync_pb::PrinterSpecifics original;
  original.set_id(kId);
  auto* ppd_reference = original.mutable_ppd_reference();
  ppd_reference->set_autoconf(true);
  ppd_reference->set_user_supplied_ppd_url(kUserSuppliedPPD);

  auto printer = SpecificsToPrinter(original);

  EXPECT_TRUE(printer->ppd_reference().autoconf);
  EXPECT_TRUE(printer->ppd_reference().user_supplied_ppd_url.empty());
  EXPECT_TRUE(printer->ppd_reference().effective_make_and_model.empty());
}

// Tests that user_supplied_ppd_url overwrites other PpdReference fields if
// autoconf is false.
TEST(SpecificsTranslationTest, UserSuppliedOverrides) {
  sync_pb::PrinterSpecifics original;
  original.set_id(kId);
  auto* ppd_reference = original.mutable_ppd_reference();
  ppd_reference->set_user_supplied_ppd_url(kUserSuppliedPPD);
  ppd_reference->set_effective_make_and_model(kEffectiveMakeAndModel);

  auto printer = SpecificsToPrinter(original);

  EXPECT_FALSE(printer->ppd_reference().autoconf);
  EXPECT_FALSE(printer->ppd_reference().user_supplied_ppd_url.empty());
  EXPECT_TRUE(printer->ppd_reference().effective_make_and_model.empty());
}

TEST(SpecificsTranslationTest, OldProtoExpectedValues) {
  sync_pb::PrinterSpecifics original;
  original.set_id(kId);
  original.set_manufacturer(kManufacturer);
  original.set_model(kModel);

  auto printer = SpecificsToPrinter(original);

  // make_and_model should be computed
  EXPECT_EQ(kMakeAndModel, printer->make_and_model());
}

TEST(SpecificsTranslationTest, OldProtoDuplicateManufacturer) {
  const std::string make = "IO";
  const std::string model = "IO Radar 2000";

  sync_pb::PrinterSpecifics original;
  original.set_id(kId);
  original.set_manufacturer(make);
  original.set_model(model);

  auto printer = SpecificsToPrinter(original);

  EXPECT_EQ("IO Radar 2000", printer->make_and_model());
}

TEST(SpecificsTranslationTest, MakeAndModelPreferred) {
  const std::string make = "UN";
  const std::string model = "EXPECTED";

  sync_pb::PrinterSpecifics original;
  original.set_id(kId);
  original.set_manufacturer(make);
  original.set_model(model);
  original.set_make_and_model(kMakeAndModel);

  auto printer = SpecificsToPrinter(original);

  EXPECT_EQ(kMakeAndModel, printer->make_and_model());
}

}  // namespace ash
