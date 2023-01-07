// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_SERVER_PRINTERS_FETCHER_H_
#define CHROME_BROWSER_ASH_PRINTING_SERVER_PRINTERS_FETCHER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/printing/printer_detector.h"

class GURL;
class Profile;

namespace ash {

enum PrintServerQueryResult {
  kNoErrors = 0,
  kIncorrectUrl,
  kConnectionError,
  kHttpError,
  kCannotParseIppResponse
};

// Instances of this class are used to query list of available printers from
// given print server. The constructor posts to an internal task runner
// a procedure responsible for building and sending the query request. When the
// response is obtained and parsed, the callback passed as a constructor's
// parameter is posted on the same sequence that constructor was called from.
// The object can be safely destroyed during query execution and on any
// sequence/thread.
class ServerPrintersFetcher {
 public:
  // The callback: |sender| is a pointer to the object calling the callback,
  // |printers| is a list of queried printers.
  using OnPrintersFetchedCallback = base::RepeatingCallback<void(
      const ServerPrintersFetcher* sender,
      const GURL& server_url,
      std::vector<PrinterDetector::DetectedPrinter>&& printers)>;

  ServerPrintersFetcher(Profile* profile,
                        const GURL& server_url,
                        const std::string& server_name,
                        OnPrintersFetchedCallback cb);

  ServerPrintersFetcher(const ServerPrintersFetcher&) = delete;
  ServerPrintersFetcher& operator=(const ServerPrintersFetcher&) = delete;

  virtual ~ServerPrintersFetcher();

  PrintServerQueryResult GetLastError() const;

 private:
  // Forward declaration of a type of an internal object.
  class PrivateImplementation;
  // Deleter of the internal object (called when unique_ptr is deleted).
  struct PimDeleter {
    void operator()(PrivateImplementation* pim) const;
  };
  // Internal object.
  std::unique_ptr<PrivateImplementation, PimDeleter> pim_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_SERVER_PRINTERS_FETCHER_H_
