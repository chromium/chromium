// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_FACTORY_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace input_method {

class EditorMediatorFactory : public ProfileKeyedServiceFactory {
 public:
  static EditorMediator* GetForProfile(Profile* profile);
  static EditorMediatorFactory* GetInstance();
  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<EditorMediatorFactory>;

  EditorMediatorFactory();
  ~EditorMediatorFactory() override;

  // BrowserContextKeyedServiceFactory overrides
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_FACTORY_H_
