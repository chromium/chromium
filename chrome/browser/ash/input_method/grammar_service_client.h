// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_GRAMMAR_SERVICE_CLIENT_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_GRAMMAR_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/machine_learning/public/mojom/grammar_checker.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom-shared.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ime/grammar_fragment.h"

namespace ash {
namespace input_method {

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
  virtual ~GrammarServiceClient();

  using TextCheckCompleteCallback = base::OnceCallback<void(
      bool /* success */,
      const std::vector<ui::GrammarFragment>& /* results */)>;

  // Sends grammar check request to ML service, parses the reponse
  // and calls a provided callback method.
  virtual bool RequestTextCheck(Profile* profile,
                                const std::u16string& text,
                                TextCheckCompleteCallback callback);

 private:
  void OnLoadGrammarCheckerDone(
      chromeos::machine_learning::mojom::GrammarCheckerQueryPtr query,
      const std::string& query_text,
      TextCheckCompleteCallback callback,
      chromeos::machine_learning::mojom::LoadModelResult result);

  void OnLoadTextClassifierDone(
      const std::string& query_text,
      TextCheckCompleteCallback callback,
      chromeos::machine_learning::mojom::LoadModelResult result);

  void OnLanguageDetectionDone(
      const std::string& query_text,
      TextCheckCompleteCallback callback,
      std::vector<chromeos::machine_learning::mojom::TextLanguagePtr>
          languages);

  // Parse the result returned from grammar check service.
  void ParseGrammarCheckerResult(
      const std::string& query_text,
      TextCheckCompleteCallback callback,
      chromeos::machine_learning::mojom::GrammarCheckerResultPtr result) const;

  // Returns whether the grammar service is enabled by user settings and the
  // service is ready to use.
  bool IsAvailable(Profile* profile) const;

  base::WeakPtr<GrammarServiceClient> weak_this_;
  mojo::Remote<chromeos::machine_learning::mojom::GrammarChecker>
      grammar_checker_;
  bool grammar_checker_loaded_ = false;
  mojo::Remote<chromeos::machine_learning::mojom::TextClassifier>
      text_classifier_;
  bool text_classifier_loaded_ = false;
  base::WeakPtrFactory<GrammarServiceClient> weak_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_GRAMMAR_SERVICE_CLIENT_H_
