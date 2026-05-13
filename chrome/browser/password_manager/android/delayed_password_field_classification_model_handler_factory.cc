// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/delayed_password_field_classification_model_handler_factory.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_field_classification_model_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/browser_context.h"

// static
DelayedPasswordFieldClassificationModelHandlerFactory*
DelayedPasswordFieldClassificationModelHandlerFactory::GetInstance() {
  static base::NoDestructor<
      DelayedPasswordFieldClassificationModelHandlerFactory>
      instance;
  return instance.get();
}

DelayedPasswordFieldClassificationModelHandlerFactory::
    DelayedPasswordFieldClassificationModelHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "DelayedPasswordFieldClassificationModelHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PasswordFieldClassificationModelHandlerFactory::GetInstance());
}

DelayedPasswordFieldClassificationModelHandlerFactory::
    ~DelayedPasswordFieldClassificationModelHandlerFactory() = default;

std::unique_ptr<KeyedService>
DelayedPasswordFieldClassificationModelHandlerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<Profile> weak_profile) {
            if (weak_profile) {
              PasswordFieldClassificationModelHandlerFactory::
                  GetForBrowserContext(weak_profile.get());
            }
          },
          profile->GetWeakPtr()),
      base::Seconds(5));

  return nullptr;
}

bool DelayedPasswordFieldClassificationModelHandlerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
