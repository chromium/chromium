// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_event_tracker.h"

#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"

namespace ash {
namespace {

// Convert from string to PdfVersion.
metrics::PrinterEventProto::PdfVersion PdfVersionFromString(
    const std::string& in) {
  if (in == "adobe-1.3") {
    return metrics::PrinterEventProto::ADOBE_1_3;
  } else if (in == "adobe-1.4") {
    return metrics::PrinterEventProto::ADOBE_1_4;
  } else if (in == "adobe-1.5") {
    return metrics::PrinterEventProto::ADOBE_1_5;
  } else if (in == "adobe-1.6") {
    return metrics::PrinterEventProto::ADOBE_1_6;
  } else if (in == "adobe-1.7") {
    return metrics::PrinterEventProto::ADOBE_1_7;
  } else if (in == "iso-15930-1_2001") {
    return metrics::PrinterEventProto::ISO_15930_1_2001;
  } else if (in == "iso-15930-3_2002") {
    return metrics::PrinterEventProto::ISO_15930_3_2002;
  } else if (in == "iso-15930-4_2003") {
    return metrics::PrinterEventProto::ISO_15930_4_2003;
  } else if (in == "iso-15930-6_2003") {
    return metrics::PrinterEventProto::ISO_15930_6_2003;
  } else if (in == "iso-15930-7_2010") {
    return metrics::PrinterEventProto::ISO_15930_7_2010;
  } else if (in == "iso-15930-8_2010") {
    return metrics::PrinterEventProto::ISO_15930_8_2010;
  } else if (in == "iso-16612-2:2010") {
    return metrics::PrinterEventProto::ISO_16612_2_2010;
  } else if (in == "iso-19005-1_2005") {
    return metrics::PrinterEventProto::ISO_19005_1_2005;
  } else if (in == "iso-19005-2_2011") {
    return metrics::PrinterEventProto::ISO_19005_2_2011;
  } else if (in == "iso-19005-3_2012") {
    return metrics::PrinterEventProto::ISO_19005_3_2012;
  } else if (in == "iso-23504-1_202") {
    return metrics::PrinterEventProto::ISO_23504_1_2020;
  } else if (in == "iso-32000-1_2008") {
    return metrics::PrinterEventProto::ISO_32000_1_2008;
  } else if (in == "iso-32000-2_2017") {
    return metrics::PrinterEventProto::ISO_32000_2_2017;
  } else if (in == "pwg-5102.3") {
    return metrics::PrinterEventProto::PWG_5102_3;
  }

  return metrics::PrinterEventProto::PDF_VERSION_UNKNOWN;
}

// Convert from string to IppFeature.
metrics::PrinterEventProto::IppFeature IppFeatureFromString(
    const std::string& in) {
  if (in == "adf") {
    return metrics::PrinterEventProto::ADF;
  } else if (in == "film-reader") {
    return metrics::PrinterEventProto::FILM_READER;
  } else if (in == "platen") {
    return metrics::PrinterEventProto::PLATEN;
  } else if (in == "document-object") {
    return metrics::PrinterEventProto::DOCUMENT_OBJECT;
  } else if (in == "faxout") {
    return metrics::PrinterEventProto::FAXOUT;
  } else if (in == "icc-color-matching") {
    return metrics::PrinterEventProto::ICC_COLOR_MATCHING;
  } else if (in == "infrastructure-printer") {
    return metrics::PrinterEventProto::INFRASTRUCTURE_PRINTER;
  } else if (in == "ipp-3d") {
    return metrics::PrinterEventProto::IPP_3D;
  } else if (in == "ipp-everywhere") {
    return metrics::PrinterEventProto::IPP_EVERYWHERE;
  } else if (in == "ipp-everywhere-server") {
    return metrics::PrinterEventProto::IPP_EVERYWHERE_SERVER;
  } else if (in == "job-release") {
    return metrics::PrinterEventProto::JOB_RELEASE;
  } else if (in == "job-save") {
    return metrics::PrinterEventProto::JOB_SAVE;
  } else if (in == "job-storage") {
    return metrics::PrinterEventProto::JOB_STORAGE;
  } else if (in == "none") {
    return metrics::PrinterEventProto::NONE;
  } else if (in == "page-overrides") {
    return metrics::PrinterEventProto::PAGE_OVERRIDES;
  } else if (in == "print-policy") {
    return metrics::PrinterEventProto::PRINT_POLICY;
  } else if (in == "production") {
    return metrics::PrinterEventProto::PRODUCTION;
  } else if (in == "proof-and-suspend") {
    return metrics::PrinterEventProto::PROOF_AND_SUSPEND;
  } else if (in == "proof-print") {
    return metrics::PrinterEventProto::PROOF_PRINT;
  } else if (in == "resource-object") {
    return metrics::PrinterEventProto::RESOURCE_OBJECT;
  } else if (in == "scan") {
    return metrics::PrinterEventProto::SCAN;
  } else if (in == "subscription-object") {
    return metrics::PrinterEventProto::SUBSCRIPTION_OBJECT;
  } else if (in == "system-object") {
    return metrics::PrinterEventProto::SYSTEM_OBJECT;
  } else if (in == "wfds-print-1.0") {
    return metrics::PrinterEventProto::WFDS_PRINT_1_0;
  } else if (in == "airprint-1.6") {
    return metrics::PrinterEventProto::AIRPRINT_1_6;
  } else if (in == "airprint-1.7") {
    return metrics::PrinterEventProto::AIRPRINT_1_7;
  } else if (in == "airprint-1.8") {
    return metrics::PrinterEventProto::AIRPRINT_1_8;
  } else if (in == "airprint-2.1") {
    return metrics::PrinterEventProto::AIRPRINT_2_1;
  }

  return metrics::PrinterEventProto::IPP_FEATURE_UNKNOWN;
}

// Set the event_type on |event| based on |mode|.
void SetEventType(metrics::PrinterEventProto* event,
                  PrinterEventTracker::SetupMode mode) {
  switch (mode) {
    case PrinterEventTracker::kUnknownMode:
      event->set_event_type(metrics::PrinterEventProto::UNKNOWN);
      break;
    case PrinterEventTracker::kUser:
      event->set_event_type(metrics::PrinterEventProto::SETUP_MANUAL);
      break;
    case PrinterEventTracker::kAutomatic:
      event->set_event_type(metrics::PrinterEventProto::SETUP_AUTOMATIC);
      break;
  }
}

// Populate PPD information in |event| based on |ppd|.
void SetPpdInfo(metrics::PrinterEventProto* event,
                const chromeos::Printer::PpdReference& ppd) {
  if (!ppd.user_supplied_ppd_url.empty()) {
    event->set_user_ppd(true);
  } else if (!ppd.effective_make_and_model.empty()) {
    event->set_ppd_identifier(ppd.effective_make_and_model);
  }
  // PPD information is not populated for autoconfigured printers.
}

// Add information to |event| specific to |usb_printer|.
void SetUsbInfo(metrics::PrinterEventProto* event,
                const chromeos::PrinterSearchData& ppd_search_data) {
  event->set_usb_vendor_id(ppd_search_data.usb_vendor_id);
  event->set_usb_model_id(ppd_search_data.usb_product_id);
  event->set_usb_printer_manufacturer(ppd_search_data.usb_manufacturer);
  event->set_usb_printer_model(ppd_search_data.usb_model);
}

// Add information to the |event| that only network printers have.
void SetNetworkPrinterInfo(metrics::PrinterEventProto* event,
                           const chromeos::Printer& printer) {
  if (!printer.make_and_model().empty()) {
    event->set_ipp_make_and_model(printer.make_and_model());
  }
}

// Add information to the `event` that only IPP printers have.
void SetIppPrinterInfo(metrics::PrinterEventProto* event,
                       const chromeos::IppPrinterInfo& info) {
  if (!info.document_formats.empty()) {
    event->mutable_document_format_supported()->Assign(
        info.document_formats.begin(), info.document_formats.end());
  }
  if (!info.document_format_preferred.empty()) {
    event->set_document_format_preferred(info.document_format_preferred);
  }
  if (!info.document_format_default.empty()) {
    event->set_document_format_default(info.document_format_default);
  }
  if (!info.urf_supported.empty()) {
    event->mutable_urf_supported()->Assign(info.urf_supported.begin(),
                                           info.urf_supported.end());
  }
  if (!info.pdf_versions.empty()) {
    for (const auto& version : info.pdf_versions) {
      event->add_pdf_versions_supported(PdfVersionFromString(version));
    }
  }
  if (!info.ipp_features.empty()) {
    for (const auto& feature : info.ipp_features) {
      event->add_ipp_features_supported(IppFeatureFromString(feature));
    }
  }
  if (!info.mopria_certified.empty()) {
    event->set_mopria_certified(info.mopria_certified);
  }
  if (!info.printer_kind.empty()) {
    event->mutable_printer_kind()->Assign(info.printer_kind.begin(),
                                          info.printer_kind.end());
  }
}

}  // namespace

PrinterEventTracker::PrinterEventTracker() = default;
PrinterEventTracker::~PrinterEventTracker() = default;

void PrinterEventTracker::set_logging(bool logging) {
  base::AutoLock l(lock_);
  logging_ = logging;
}

void PrinterEventTracker::RecordUsbPrinterInstalled(
    const chromeos::Printer::PpdReference& ppd_reference,
    const chromeos::PrinterSearchData& ppd_search_data,
    SetupMode mode) {
  base::AutoLock l(lock_);
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  SetEventType(&event, mode);
  SetPpdInfo(&event, ppd_reference);
  SetUsbInfo(&event, ppd_search_data);
  events_.push_back(event);
}

void PrinterEventTracker::RecordIppPrinterInstalled(
    const chromeos::Printer& printer,
    SetupMode mode,
    const std::optional<chromeos::IppPrinterInfo>& ipp_printer_info) {
  base::AutoLock l(lock_);
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  SetEventType(&event, mode);
  SetPpdInfo(&event, printer.ppd_reference());
  SetNetworkPrinterInfo(&event, printer);
  if (ipp_printer_info.has_value()) {
    SetIppPrinterInfo(&event, ipp_printer_info.value());
  }
  events_.push_back(event);
}

void PrinterEventTracker::RecordUsbSetupAbandoned(
    const chromeos::PrinterSearchData& ppd_search_data) {
  base::AutoLock l(lock_);
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  event.set_event_type(metrics::PrinterEventProto::SETUP_ABANDONED);
  SetUsbInfo(&event, ppd_search_data);
  events_.push_back(event);
}

void PrinterEventTracker::RecordSetupAbandoned(
    const chromeos::Printer& printer) {
  base::AutoLock l(lock_);
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  event.set_event_type(metrics::PrinterEventProto::SETUP_ABANDONED);
  SetNetworkPrinterInfo(&event, printer);
  events_.push_back(event);
}

void PrinterEventTracker::RecordPrinterRemoved(
    const chromeos::Printer& printer) {
  base::AutoLock l(lock_);
  if (!logging_) {
    return;
  }

  metrics::PrinterEventProto event;
  event.set_event_type(metrics::PrinterEventProto::PRINTER_DELETED);
  SetNetworkPrinterInfo(&event, printer);
  SetPpdInfo(&event, printer.ppd_reference());
  events_.push_back(event);
}

void PrinterEventTracker::FlushPrinterEvents(
    std::vector<metrics::PrinterEventProto>* events) {
  base::AutoLock l(lock_);
  events->swap(events_);
  events_.clear();
}

}  // namespace ash
