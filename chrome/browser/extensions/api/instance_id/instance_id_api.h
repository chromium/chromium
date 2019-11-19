// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INSTANCE_ID_INSTANCE_ID_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INSTANCE_ID_INSTANCE_ID_API_H_

#include "base/macros.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class InstanceIDApiFunction : public ExtensionFunction {
 public:
  InstanceIDApiFunction();

 protected:
  ~InstanceIDApiFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Actual implementation of specific functions.
  virtual ResponseAction DoWork() = 0;

  // Checks whether the InstanceID API is enabled.
  bool IsEnabled() const;

  instance_id::InstanceID* GetInstanceID() const;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDApiFunction);
};

class InstanceIDGetIDFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.getID", INSTANCEID_GETID)

  InstanceIDGetIDFunction();

 protected:
  ~InstanceIDGetIDFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void GetIDCompleted(const std::string& id);

  DISALLOW_COPY_AND_ASSIGN(InstanceIDGetIDFunction);
};

class InstanceIDGetCreationTimeFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.getCreationTime",
                             INSTANCEID_GETCREATIONTIME)

  InstanceIDGetCreationTimeFunction();

 protected:
  ~InstanceIDGetCreationTimeFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void GetCreationTimeCompleted(const base::Time& creation_time);

  DISALLOW_COPY_AND_ASSIGN(InstanceIDGetCreationTimeFunction);
};

class InstanceIDGetTokenFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.getToken", INSTANCEID_GETTOKEN)

  InstanceIDGetTokenFunction();

 protected:
  ~InstanceIDGetTokenFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void GetTokenCompleted(const std::string& token,
                         instance_id::InstanceID::Result result);

  DISALLOW_COPY_AND_ASSIGN(InstanceIDGetTokenFunction);
};

class InstanceIDDeleteTokenFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.deleteToken", INSTANCEID_DELETETOKEN)

  InstanceIDDeleteTokenFunction();

 protected:
  ~InstanceIDDeleteTokenFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void DeleteTokenCompleted(instance_id::InstanceID::Result result);

  DISALLOW_COPY_AND_ASSIGN(InstanceIDDeleteTokenFunction);
};

class InstanceIDDeleteIDFunction : public InstanceIDApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("instanceID.deleteID", INSTANCEID_DELETEID)

  InstanceIDDeleteIDFunction();

 protected:
  ~InstanceIDDeleteIDFunction() override;

  // InstanceIDApiFunction:
  ResponseAction DoWork() override;

 private:
  void DeleteIDCompleted(instance_id::InstanceID::Result result);

  DISALLOW_COPY_AND_ASSIGN(InstanceIDDeleteIDFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INSTANCE_ID_INSTANCE_ID_API_H_
