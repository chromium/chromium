// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_XPS_FEATURES_H_
#define CHROME_BROWSER_PRINTING_XPS_FEATURES_H_

namespace printing {

// This file contains queries to determine behavior related to the XPS printing
// feature, as to whether the feature itself or certain parts of it should be
// enabled or not.  It encapsulates the results from feature flags and any
// policy overrides.

// Helper function to determine if there is any print path which could require
// the use of XPS print capabilities.
bool IsXpsPrintCapabilityRequired();

// Helper function to determine if printing of a document from a particular
// source should be done using XPS printing API instead of with GDI.
bool ShouldPrintUsingXps(bool source_is_pdf);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_XPS_FEATURES_H_
