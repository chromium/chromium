// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INSTANCE_ID_INSTANCE_ID_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INSTANCE_ID_INSTANCE_ID_API_H_

#include "components/gcm_driver/instance_id/instance_id.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class InstanceIDApiFunction : public ExtensionFunction {
 public:
  InstanceIDApiFunction();

  InstanceIDApiFunction(const InstanceIDApiFunction&) = delete;
  InstanceIDApiFunction& operator=(const InstanceIDApiFunction&) = delete;

 protected:
  ~InstanceIDApiFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Actual implementation of specific functions.
  virtual ResponseAction DoWork() = 0;

  // Checks whether the InstanceID API is enabled.
  bool IsEnabled() const;

  instance_id::InstanceID* GetInstanceID() const;
};

class InstanceIDGetIDFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.getID", INSTANCEID_GETID)

  InstanceIDGetIDFunction();

  InstanceIDGetIDFunction(const InstanceIDGetIDFunction&) = delete;
  InstanceIDGetIDFunction& operator=(const InstanceIDGetIDFunction&) = delete;

 protected:
  ~InstanceIDGetIDFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void GetIDCompleted(const std::string& id);
};

class InstanceIDGetCreationTimeFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.getCreationTime",
                             INSTANCEID_GETCREATIONTIME)

  InstanceIDGetCreationTimeFunction();

  InstanceIDGetCreationTimeFunction(const InstanceIDGetCreationTimeFunction&) =
      delete;
  InstanceIDGetCreationTimeFunction& operator=(
      const InstanceIDGetCreationTimeFunction&) = delete;

 protected:
  ~InstanceIDGetCreationTimeFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void GetCreationTimeCompleted(const base::Time& creation_time);
};

class InstanceIDGetTokenFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.getToken", INSTANCEID_GETTOKEN)

  InstanceIDGetTokenFunction();

  InstanceIDGetTokenFunction(const InstanceIDGetTokenFunction&) = delete;
  InstanceIDGetTokenFunction& operator=(const InstanceIDGetTokenFunction&) =
      delete;

 protected:
  ~InstanceIDGetTokenFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void GetTokenCompleted(const std::string& token,
                         instance_id::InstanceID::Result result);
};

class InstanceIDDeleteTokenFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.deleteToken", INSTANCEID_DELETETOKEN)

  InstanceIDDeleteTokenFunction();

  InstanceIDDeleteTokenFunction(const InstanceIDDeleteTokenFunction&) = delete;
  InstanceIDDeleteTokenFunction& operator=(
      const InstanceIDDeleteTokenFunction&) = delete;

 protected:
  ~InstanceIDDeleteTokenFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void DeleteTokenCompleted(instance_id::InstanceID::Result result);
};

class InstanceIDDeleteIDFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.deleteID", INSTANCEID_DELETEID)

  InstanceIDDeleteIDFunction();

  InstanceIDDeleteIDFunction(const InstanceIDDeleteIDFunction&) = delete;
  InstanceIDDeleteIDFunction& operator=(const InstanceIDDeleteIDFunction&) =
      delete;

 protected:
  ~InstanceIDDeleteIDFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void DeleteIDCompleted(instance_id::InstanceID::Result result);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INSTANCE_ID_INSTANCE_ID_API_H_
