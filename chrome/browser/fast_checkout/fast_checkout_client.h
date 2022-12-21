// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_H_

class GURL;

namespace autofill {
class AutofillDriver;
struct FormData;
struct FormFieldData;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

// Abstract interface for handling a fast checkout run.
class FastCheckoutClient {
 public:
  FastCheckoutClient(const FastCheckoutClient&) = delete;
  FastCheckoutClient& operator=(const FastCheckoutClient&) = delete;

  // Factory method for creating a `FastCheckoutClient` instance.
  static FastCheckoutClient* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Starts the fast checkout run. Returns true if the run was successful.
  virtual bool TryToStart(const GURL& url,
                          const autofill::FormData& form,
                          const autofill::FormFieldData& field,
                          autofill::AutofillDriver* autofill_driver) = 0;

  // Stops the fast checkout run. Resets internal UI state to `kNotShownYet` if
  // `allow_further_runs == true`.
  virtual void Stop(bool allow_further_runs) = 0;

  // Returns true if a fast checkout run is ongoing.
  virtual bool IsRunning() const = 0;

  // Returns true if the bottomsheet is currently showing to the user.
  virtual bool IsShowing() const = 0;

 protected:
  FastCheckoutClient() = default;
  virtual ~FastCheckoutClient() = default;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_H_
