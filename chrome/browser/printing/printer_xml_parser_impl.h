// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_XML_PARSER_IMPL_H_
#define CHROME_BROWSER_PRINTING_PRINTER_XML_PARSER_IMPL_H_

#include <memory>

#include "chrome/services/printing/public/mojom/printer_xml_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "printing/mojom/print.mojom.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace printing {

class PrinterXmlParserImpl : public mojom::PrinterXmlParser {
 public:
  using ParseXmlCallback =
      base::OnceCallback<void(mojom::PrinterCapabilitiesValueResultPtr)>;

  PrinterXmlParserImpl();
  PrinterXmlParserImpl(const PrinterXmlParserImpl&) = delete;
  PrinterXmlParserImpl& operator=(const PrinterXmlParserImpl&) = delete;
  ~PrinterXmlParserImpl() override;

  void ParseXmlForPrinterCapabilities(const std::string& capabilities_xml,
                                      ParseXmlCallback callback) override;

  mojo::PendingRemote<mojom::PrinterXmlParser> GetRemote();

 private:
  mojo::ReceiverSet<mojom::PrinterXmlParser> receivers_;
  std::unique_ptr<data_decoder::DataDecoder> decoder_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_XML_PARSER_IMPL_H_
