// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_JS_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_JS_CHECKER_H_

#include <string>

namespace content {
class WebContents;
}

namespace chromeos {
namespace test {

// Utility class for tests that allows us to evalute and check JavaScript
// expressions inside given web contents. All calls are made synchronously.
class JSChecker {
 public:
  JSChecker();
  explicit JSChecker(content::WebContents* web_contents);

  // Evaluates |expression|. Evaluation will be completed when this function
  // call returns.
  void Evaluate(const std::string& expression);

  // Executes |expression|. Doesn't require a correct command. Command will be
  // queued up and executed later. This function will return immediately.
  void ExecuteAsync(const std::string& expression);

  // Evaluates |expression| and returns its result.
  bool GetBool(const std::string& expression);
  int GetInt(const std::string& expression);
  std::string GetString(const std::string& expression);

  // Checks truthfulness of the given |expression|.
  void ExpectTrue(const std::string& expression);
  void ExpectFalse(const std::string& expression);

  // Compares result of |expression| with |result|.
  void ExpectEQ(const std::string& expression, int result);
  void ExpectNE(const std::string& expression, int result);
  void ExpectEQ(const std::string& expression, const std::string& result);
  void ExpectNE(const std::string& expression, const std::string& result);
  void ExpectEQ(const std::string& expression, bool result);
  void ExpectNE(const std::string& expression, bool result);

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  void GetBoolImpl(const std::string& expression, bool* result);
  void GetIntImpl(const std::string& expression, int* result);
  void GetStringImpl(const std::string& expression, std::string* result);

  content::WebContents* web_contents_;
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_JS_CHECKER_H_
