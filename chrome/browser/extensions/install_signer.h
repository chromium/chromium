// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_SIGNER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_SIGNER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace extensions {

// This represents a list of ids signed with a private key using an algorithm
// that includes some salt bytes.
struct InstallSignature {
  // The set of ids that have been signed.
  ExtensionIdSet ids;

  // Both of these are just arrays of bytes, NOT base64-encoded.
  std::string salt;
  std::string signature;

  // The date that the signature should expire, in YYYY-MM-DD format.
  std::string expire_date;

  // The time this signature was obtained from the server. Note that this
  // is computed locally and *not* signed by the server key.
  base::Time timestamp;

  // The set of ids that the server indicated were invalid (ie not signed).
  // Note that this is computed locally and *not* signed by the signature.
  ExtensionIdSet invalid_ids;

  InstallSignature();
  InstallSignature(const InstallSignature& other);
  ~InstallSignature();

  // Helper methods for serialization to/from a base::Value::Dict.
  [[nodiscard]] base::Value::Dict ToDict() const;

  static std::unique_ptr<InstallSignature> FromDict(
      const base::Value::Dict& dict);
};

// Objects of this class encapsulate an operation to get a signature proving
// that a set of ids are hosted in the webstore.
class InstallSigner {
 public:
  using SignatureCallback =
      base::OnceCallback<void(std::unique_ptr<InstallSignature>)>;

  // IMPORTANT NOTE: It is possible that only some, but not all, of the entries
  // in |ids| will be successfully signed by the backend. Callers should always
  // check the set of ids in the InstallSignature passed to their callback, as
  // it may contain only a subset of the ids they passed in.
  InstallSigner(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const ExtensionIdSet& ids);

  InstallSigner(const InstallSigner&) = delete;
  InstallSigner& operator=(const InstallSigner&) = delete;

  ~InstallSigner();

  // Returns a set of ids that are forced to be considered not from webstore,
  // e.g. by a command line flag used for testing.
  static ExtensionIdSet GetForcedNotFromWebstore();

  // Begins the process of fetching a signature from the backend. This should
  // only be called once! If you want to get another signature, make another
  // instance of this class.
  void GetSignature(SignatureCallback callback);

  // Returns whether the signature in InstallSignature is properly signed with a
  // known public key.
  static bool VerifySignature(const InstallSignature& signature);

 private:

  // A helper function that calls |callback_| with an indication that an error
  // happened (currently done by passing an empty pointer).
  void ReportErrorViaCallback();

  // Called when |simple_loader_| has returned a result to parse the response,
  // and then call HandleSignatureResult with structured data.
  void ParseFetchResponse(std::unique_ptr<std::string> response_body);

  // Handles the result from a backend fetch.
  void HandleSignatureResult(const std::string& signature,
                             const std::string& expire_date,
                             const ExtensionIdSet& invalid_ids);

  // The final callback for when we're done.
  SignatureCallback callback_;

  // The current set of ids we're trying to verify. This may contain fewer ids
  // than we started with.
  ExtensionIdSet ids_;

  // An array of random bytes used as an input to hash with the machine id,
  // which will need to be persisted in the eventual InstallSignature we get.
  std::string salt_;

  // These are used to make the call to a backend server for a signature.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // The underlying SimpleURLLoader which does the actual load.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // The time the request to the server was started.
  base::Time request_start_time_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_SIGNER_H_
