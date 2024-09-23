// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_EXTRA_INSTANCES_H_
#define ASH_COMPONENTS_KCER_EXTRA_INSTANCES_H_

#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_impl.h"
#include "base/no_destructor.h"

namespace kcer {

// This class is a singleton that provides access to some special Kcer
// instances. It is separate from KcerFactory primarily for the code under
// */components/* and other places that don't have access to Profile-s and
// cannot use KcerFactory. Using KcerFactory should be preferred whenever
// possible.
// TODO(miersh): This class was implemented to simplify the migration from NSS
// to Kcer. Eventually all the code should be refactored to either retrieve the
// correct Kcer instance by a Profile or receive the instance as an input. This
// class should be merged into KcerFactory after that.
class COMPONENT_EXPORT(KCER) ExtraInstances {
 public:
  static ExtraInstances* Get();

  // Retrieves the Kcer instance for the main profile (or EmptyKcer, if there's
  // none). This method is only introduced to migrate some existing code from
  // NSS to Kcer. If at all possible, all new code should either fetch Kcer from
  // KcerFactory by Profile or receive a pointer to the correct Kcer instance as
  // an argument.
  static base::WeakPtr<Kcer> GetDefaultKcer();
  // Returns a Kcer instance without any tokens.
  static base::WeakPtr<Kcer> GetEmptyKcer();
  // Returns a Kcer instance with just the device token (or with no tokens, if
  // the device token should not be available in the current environment, e.g.
  // in Lacros).
  static base::WeakPtr<Kcer> GetDeviceKcer();

  // Initializes DeviceKcer. DeviceKcer can be used immediately after creation,
  // but it won't complete any requests until it's initialized.
  void InitializeDeviceKcer(scoped_refptr<base::TaskRunner> token_task_runner,
                            base::WeakPtr<internal::KcerToken> device_token);

  // Used by KcerFactory to update the value returned from GetDefaultKcer().
  void SetDefaultKcer(base::WeakPtr<Kcer> default_kcer);

 private:
  friend class base::NoDestructor<ExtraInstances>;

  ExtraInstances();
  ~ExtraInstances();

  internal::KcerImpl empty_kcer_;
  // Only initialized in Ash.
  std::unique_ptr<internal::KcerImpl> device_kcer_;

  // Contains a nullptr on the login screen. In a user session contains Kcer for
  // the profile of the primary user. After the primary profile is destroyed,
  // the pointer is automatically invalidated.
  base::WeakPtr<Kcer> default_kcer_;
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_EXTRA_INSTANCES_H_
