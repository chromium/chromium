// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_handler_stream_delegate.h"

namespace pdf {

PdfHandlerStreamDelegate::PdfHandlerStreamDelegate() = default;
PdfHandlerStreamDelegate::~PdfHandlerStreamDelegate() = default;

bool PdfHandlerStreamDelegate::PluginCanSave() const {
  return plugin_can_save_;
}

void PdfHandlerStreamDelegate::SetPluginCanSave(bool plugin_can_save) {
  plugin_can_save_ = plugin_can_save;
}

}  // namespace pdf
