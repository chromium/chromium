// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_SERVICE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/machine_learning/public/mojom/grammar_checker.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

struct SpellCheckResult;

namespace chromeos {

// A class that sends grammar check request to ML service, parses the reponse
// and calls a provided callback method. A simple usage is creating a
// GrammarServiceClient and call its RequestTextCheck method as listed in the
// following snippet.
//
//   class MyClient {
//    public:
//     MyClient();
//     virtual ~MyClient();
//
//     void OnTextCheckComplete(
//         int tag,
//         bool success,
//         const std::vector<SpellCheckResult>& results) {
//       ...
//     }
//
//     void MyTextCheck(Profile* profile, const std::u16string& text) {
//        client_.reset(new GrammarServiceClient);
//        client_->RequestTextCheck(profile, 0, text,
//            base::BindOnce(&MyClient::OnTextCheckComplete,
//                           base::Unretained(this));
//     }
//    private:
//     std::unique_ptr<GrammarServiceClient> client_;
//   };
//
class GrammarServiceClient {
 public:
  GrammarServiceClient();
  ~GrammarServiceClient();

  using TextCheckCompleteCallback = base::OnceCallback<void(
      bool /* success */,
      const std::vector<SpellCheckResult>& /* results */)>;

  // Sends grammar check request to ML service, parses the reponse
  // and calls a provided callback method.
  bool RequestTextCheck(Profile* profile,
                        const std::u16string& text,
                        TextCheckCompleteCallback callback) const;

 private:
  // Parse the result returned from grammar check service.
  void ParseGrammarCheckerResult(
      const std::u16string& text,
      TextCheckCompleteCallback callback,
      chromeos::machine_learning::mojom::GrammarCheckerResultPtr result) const;

  // Returns whether the grammar service is enabled by user settings and the
  // service is ready to use.
  bool IsAvailable(Profile* profile) const;

  mojo::Remote<chromeos::machine_learning::mojom::GrammarChecker>
      grammar_checker_;
  bool grammar_checker_loaded_ = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_SERVICE_CLIENT_H_
