// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_map.h"

#include "base/macros.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

bool IsPrinterInPrinters(const std::vector<Printer>& printers,
                         const Printer& printer) {
  for (const auto& p : printers) {
    if (p.id() == printer.id()) {
      return true;
    }
  }
  return false;
}

}  // namespace

class PrintersMapTest : public testing::Test {
 public:
  PrintersMapTest() = default;
  ~PrintersMapTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(PrintersMapTest);
};

TEST_F(PrintersMapTest, GetAllReturnsEmptyVector) {
  PrintersMap printers_map;

  std::vector<Printer> printers = printers_map.Get();

  EXPECT_EQ(0u, printers.size());
}

TEST_F(PrintersMapTest, GetAllReturnsPrintersFromAllClasses) {
  PrintersMap printers_map;

  Printer discovered_printer = Printer("discovered_id");
  Printer enterprise_printer = Printer("enterprise_id");

  printers_map.Insert(PrinterClass::kEnterprise, enterprise_printer);
  printers_map.Insert(PrinterClass::kDiscovered, discovered_printer);

  auto printers = printers_map.Get();

  EXPECT_EQ(2u, printers.size());
  EXPECT_TRUE(IsPrinterInPrinters(printers, discovered_printer));
  EXPECT_TRUE(IsPrinterInPrinters(printers, enterprise_printer));
}

TEST_F(PrintersMapTest, GetByIdReturnsEmptyOptionalWhenNonExistant) {
  PrintersMap printers_map;

  base::Optional<Printer> printer = printers_map.Get("non_existant_id");

  EXPECT_FALSE(printer);
}

TEST_F(PrintersMapTest, GetByIdReturnsPrinterWhenExistant) {
  PrintersMap printers_map;

  Printer expected_printer = Printer("printer_id");
  expected_printer.set_display_name("123");

  printers_map.Insert(PrinterClass::kEnterprise, expected_printer);

  base::Optional<Printer> actual_printer =
      printers_map.Get(expected_printer.id());

  EXPECT_TRUE(actual_printer);
  EXPECT_EQ(expected_printer.id(), actual_printer->id());
  EXPECT_EQ(expected_printer.display_name(), actual_printer->display_name());
}

TEST_F(PrintersMapTest, GetByClassReturnsEmptyVectorWhenNonExistant) {
  PrintersMap printers_map;

  auto printers = printers_map.Get(PrinterClass::kDiscovered);

  EXPECT_EQ(0u, printers.size());
}

TEST_F(PrintersMapTest, GetByClassReturnsPrinters) {
  PrintersMap printers_map;

  Printer discovered_printer = Printer("discovered_id");
  Printer enterprise_printer = Printer("enterprise_id");

  printers_map.Insert(PrinterClass::kEnterprise, enterprise_printer);
  printers_map.Insert(PrinterClass::kDiscovered, discovered_printer);

  auto discovered_printers = printers_map.Get(PrinterClass::kDiscovered);

  EXPECT_EQ(1u, discovered_printers.size());
  EXPECT_EQ(discovered_printer.id(), discovered_printers[0].id());

  auto enterprise_printers = printers_map.Get(PrinterClass::kEnterprise);

  EXPECT_EQ(1u, enterprise_printers.size());
  EXPECT_EQ(enterprise_printer.id(), enterprise_printers[0].id());
}

TEST_F(PrintersMapTest,
       GetByClassAndIdOnlyReturnsEmptyOptionalWhenNonExistant) {
  PrintersMap printers_map;

  base::Optional<Printer> found_printer =
      printers_map.Get(PrinterClass::kDiscovered, "printer_id");

  EXPECT_FALSE(found_printer);
}

TEST_F(PrintersMapTest, GetByClassAndIdOnlyReturnsFromCorrectClass) {
  PrintersMap printers_map;

  const std::string printer1_id = "1";
  Printer printer1 = Printer(printer1_id);
  Printer printer2 = Printer("2");

  printers_map.Insert(PrinterClass::kEnterprise, printer1);
  printers_map.Insert(PrinterClass::kDiscovered, printer2);

  // No printer is found because |printer1| *is not* in kDiscovered.
  base::Optional<Printer> found_printer =
      printers_map.Get(PrinterClass::kDiscovered, printer1_id);

  EXPECT_FALSE(found_printer);

  // Printer found because |printer1| *is* in kEnterprise.
  found_printer = printers_map.Get(PrinterClass::kEnterprise, printer1_id);

  EXPECT_TRUE(found_printer);
  EXPECT_EQ(printer1_id, found_printer->id());
}

TEST_F(PrintersMapTest, InsertAddsPrinterToCorrectClass) {
  PrintersMap printers_map;

  Printer expected_printer = Printer("printer_id");
  expected_printer.set_display_name("123");

  EXPECT_EQ(0u, printers_map.Get(PrinterClass::kAutomatic).size());

  printers_map.Insert(PrinterClass::kAutomatic, expected_printer);

  EXPECT_EQ(1u, printers_map.Get(PrinterClass::kAutomatic).size());
}

TEST_F(PrintersMapTest, GetSecurePrintersReturnsEmptyVectorOnNonExistantClass) {
  PrintersMap printers_map;

  auto printers = printers_map.GetSecurePrinters();

  EXPECT_EQ(0u, printers.size());
}

TEST_F(PrintersMapTest, GetSecurePrintersOnlyReturnsSecurePrinters) {
  PrintersMap printers_map;

  Printer ipp_printer = Printer("ipp");
  ipp_printer.set_uri("ipp:printer");
  printers_map.Insert(PrinterClass::kSaved, ipp_printer);

  Printer ipps_printer = Printer("ipps");
  ipps_printer.set_uri("ipps:printer");
  printers_map.Insert(PrinterClass::kAutomatic, ipps_printer);

  Printer usb_printer = Printer("usb");
  usb_printer.set_uri("usb:printer");
  printers_map.Insert(PrinterClass::kDiscovered, usb_printer);

  Printer ippusb_printer = Printer("ippusb");
  ippusb_printer.set_uri("ippusb:printer");
  printers_map.Insert(PrinterClass::kEnterprise, ippusb_printer);

  Printer http_printer = Printer("http");
  http_printer.set_uri("http:printer");
  printers_map.Insert(PrinterClass::kDiscovered, http_printer);

  Printer https_printer = Printer("https");
  https_printer.set_uri("https://printer");
  printers_map.Insert(PrinterClass::kDiscovered, https_printer);

  // Only HTTPS, IPPS, IPPUSB, and USB printers are returned.
  auto printers = printers_map.GetSecurePrinters();

  EXPECT_EQ(4u, printers.size());

  EXPECT_TRUE(IsPrinterInPrinters(printers, ipps_printer));
  EXPECT_TRUE(IsPrinterInPrinters(printers, usb_printer));
  EXPECT_TRUE(IsPrinterInPrinters(printers, ippusb_printer));
  EXPECT_TRUE(IsPrinterInPrinters(printers, https_printer));

  EXPECT_FALSE(IsPrinterInPrinters(printers, ipp_printer));
  EXPECT_FALSE(IsPrinterInPrinters(printers, http_printer));
}

TEST_F(PrintersMapTest,
       GetSecurePrintersInClassReturnsEmptyVectorOnNonExistantClass) {
  PrintersMap printers_map;

  auto printers = printers_map.GetSecurePrinters(PrinterClass::kAutomatic);

  EXPECT_EQ(0u, printers.size());
}

TEST_F(PrintersMapTest, GetSecurePrintersInClassOnlyReturnsSecurePrinters) {
  PrintersMap printers_map;

  Printer ipp_printer = Printer("ipp");
  ipp_printer.set_uri("ipp:printer");
  printers_map.Insert(PrinterClass::kSaved, ipp_printer);

  Printer ipps_printer = Printer("ipps");
  ipps_printer.set_uri("ipps:printer");
  printers_map.Insert(PrinterClass::kSaved, ipps_printer);

  Printer usb_printer = Printer("usb");
  usb_printer.set_uri("usb:printer");
  printers_map.Insert(PrinterClass::kSaved, usb_printer);

  Printer ippusb_printer = Printer("ippusb");
  ippusb_printer.set_uri("ippusb:printer");
  printers_map.Insert(PrinterClass::kSaved, ippusb_printer);

  Printer http_printer = Printer("http");
  http_printer.set_uri("http:printer");
  printers_map.Insert(PrinterClass::kSaved, http_printer);

  // Only IPPS, IPPUSB, and USB printers are returned.
  auto printers = printers_map.GetSecurePrinters(PrinterClass::kSaved);

  EXPECT_EQ(3u, printers.size());

  EXPECT_TRUE(IsPrinterInPrinters(printers, ipps_printer));
  EXPECT_TRUE(IsPrinterInPrinters(printers, usb_printer));
  EXPECT_TRUE(IsPrinterInPrinters(printers, ippusb_printer));

  EXPECT_FALSE(IsPrinterInPrinters(printers, ipp_printer));
  EXPECT_FALSE(IsPrinterInPrinters(printers, http_printer));
}

TEST_F(PrintersMapTest, ClearResetsClass) {
  PrintersMap printers_map;

  Printer printer1 = Printer("id1");
  Printer printer2 = Printer("id2");

  printers_map.Insert(PrinterClass::kEnterprise, printer1);
  printers_map.Insert(PrinterClass::kEnterprise, printer2);

  auto enterprise_printers = printers_map.Get(PrinterClass::kEnterprise);
  EXPECT_EQ(2u, enterprise_printers.size());

  // Calling clear erases the contents of the class.
  printers_map.Clear(PrinterClass::kEnterprise);

  auto empty_printers = printers_map.Get(PrinterClass::kEnterprise);
  EXPECT_EQ(0u, empty_printers.size());
}

TEST_F(PrintersMapTest, ReplacePrintersInClassOverwritesPrinters) {
  PrintersMap printers_map;

  Printer printer1 = Printer("id1");
  Printer printer2 = Printer("id2");

  printers_map.Insert(PrinterClass::kEnterprise, printer1);
  printers_map.Insert(PrinterClass::kEnterprise, printer2);

  auto enterprise_printers = printers_map.Get(PrinterClass::kEnterprise);
  EXPECT_EQ(2u, enterprise_printers.size());

  // Overwriting the printer class with an empty vector should clear the
  // printers.
  printers_map.ReplacePrintersInClass(PrinterClass::kEnterprise,
                                      std::vector<Printer>());

  auto empty_printers = printers_map.Get(PrinterClass::kEnterprise);
  EXPECT_EQ(0u, empty_printers.size());

  // Replacing the printer class with the original enterprise printers restores
  // them.
  printers_map.ReplacePrintersInClass(PrinterClass::kEnterprise,
                                      enterprise_printers);

  auto restored_printers = printers_map.Get(PrinterClass::kEnterprise);
  EXPECT_EQ(2u, restored_printers.size());
}

TEST_F(PrintersMapTest, RemoveSucceedsOnPrinterInClass) {
  PrintersMap printers_map;

  const std::string printer_id = "id1";

  printers_map.Insert(PrinterClass::kEnterprise, Printer(printer_id));

  auto printer = printers_map.Get(printer_id);
  EXPECT_TRUE(printer);
  EXPECT_EQ(printer_id, printer->id());

  printers_map.Remove(PrinterClass::kEnterprise, printer_id);

  printer = printers_map.Get(printer_id);
  EXPECT_FALSE(printer);
}

TEST_F(PrintersMapTest, RemoveDoesNothingOnPrinterNotInClass) {
  PrintersMap printers_map;

  const std::string printer_id = "id1";

  printers_map.Insert(PrinterClass::kEnterprise, Printer(printer_id));

  auto printer = printers_map.Get(printer_id);
  EXPECT_TRUE(printer);
  EXPECT_EQ(printer_id, printer->id());

  // Call remove using a different class, same printer_id and verify the printer
  // is not rmemoved.
  printers_map.Remove(PrinterClass::kDiscovered, printer_id);

  printer = printers_map.Get(printer_id);
  EXPECT_TRUE(printer);
  EXPECT_EQ(printer_id, printer->id());
}

TEST_F(PrintersMapTest, RemoveDoesNothingOnUnknownPrinter) {
  PrintersMap printers_map;

  const std::string printer_id = "id";

  printers_map.Insert(PrinterClass::kEnterprise, Printer(printer_id));

  EXPECT_TRUE(printers_map.Get(printer_id));

  // Call remove using a printer that does not exist in any class and verify the
  // other printers are not changed.
  printers_map.Remove(PrinterClass::kDiscovered, "random_id");

  EXPECT_TRUE(printers_map.Get(printer_id));
}

TEST_F(PrintersMapTest, IsPrinterInClass) {
  PrintersMap printers_map;

  const std::string printer_id = "id";

  // Returns false for non-existent printers.
  EXPECT_FALSE(
      printers_map.IsPrinterInClass(PrinterClass::kEnterprise, "random_id"));

  // Add an enterprise printer. It can be found as an enterprise printer, but
  // not as a discovered printer.
  printers_map.Insert(PrinterClass::kEnterprise, Printer(printer_id));
  EXPECT_TRUE(
      printers_map.IsPrinterInClass(PrinterClass::kEnterprise, printer_id));
  EXPECT_FALSE(
      printers_map.IsPrinterInClass(PrinterClass::kDiscovered, printer_id));
}

}  // namespace chromeos
