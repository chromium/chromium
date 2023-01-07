// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_xml_parser_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/services/printing/public/mojom/printer_xml_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "printing/mojom/print.mojom.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace printing {

namespace {

void XmlPrinterCapabilitiesParsed(
    PrinterXmlParserImpl::ParseXmlCallback callback,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  if (!value_or_error.has_value()) {
    std::move(callback).Run(
        mojom::PrinterCapabilitiesValueResult::NewResultCode(
            mojom::ResultCode::kFailed));
    return;
  }
  std::move(callback).Run(
      mojom::PrinterCapabilitiesValueResult::NewCapabilities(
          std::move(value_or_error.value())));
}

}  // namespace

PrinterXmlParserImpl::PrinterXmlParserImpl() = default;

PrinterXmlParserImpl::~PrinterXmlParserImpl() = default;

void PrinterXmlParserImpl::ParseXmlForPrinterCapabilities(
    const std::string& capabilities_xml,
    ParseXmlCallback callback) {
  if (!decoder_)
    decoder_ = std::make_unique<data_decoder::DataDecoder>();
  decoder_->ParseXml(
      capabilities_xml,
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&XmlPrinterCapabilitiesParsed, std::move(callback)));
}

mojo::PendingRemote<mojom::PrinterXmlParser> PrinterXmlParserImpl::GetRemote() {
  mojo::PendingRemote<mojom::PrinterXmlParser> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

}  // namespace printing
