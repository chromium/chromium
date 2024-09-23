// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONT_PREF_CHANGE_NOTIFIER_H_
#define CHROME_BROWSER_FONT_PREF_CHANGE_NOTIFIER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_observer.h"

class PrefService;

// There are more than 1000 font prefs and several subsystems need to be
// notified when a font changes. Registering for 1000 font change notifications
// is inefficient and slows down other pref notifications, and having each font
// subsystem registering "any preference changed" observers is also
// inefficient.
//
// This class registers for "any preference changed" notifications, filters
// for font pref changes only, and then issues notifications to registered
// observers.
//
// There is one FontPrefChangeNotifier per Profile. Construct one with the
// FontPrefChangeNotifierFactory.
class FontPrefChangeNotifier : public PrefObserver, public KeyedService {
 public:
  // The parameter is the full name of the font pref that changed.
  using Callback = base::RepeatingCallback<void(const std::string&)>;

  // Instantiate this subclass to scope an observer notification. The registrar
  // can have only one callback.
  class Registrar {
   public:
    Registrar();

    Registrar(const Registrar&) = delete;
    Registrar& operator=(const Registrar&) = delete;

    ~Registrar();

    bool is_registered() const { return !!notifier_; }

    // Start watching for changes.
    void Register(FontPrefChangeNotifier* notifier,
                  FontPrefChangeNotifier::Callback cb);

    // Optional way to unregister before the Registrar object goes out of
    // scope. The object must currently be registered.
    void Unregister();

   private:
    friend FontPrefChangeNotifier;

    raw_ptr<FontPrefChangeNotifier> notifier_ = nullptr;
    FontPrefChangeNotifier::Callback callback_;
  };

  // The pref service must outlive this class.
  explicit FontPrefChangeNotifier(PrefService* pref_service);

  FontPrefChangeNotifier(const FontPrefChangeNotifier&) = delete;
  FontPrefChangeNotifier& operator=(const FontPrefChangeNotifier&) = delete;

  ~FontPrefChangeNotifier() override;

 private:
  friend Registrar;

  void AddRegistrar(Registrar* registrar);
  void RemoveRegistrar(Registrar* registrar);

  // PrefObserver implementation.
  void OnPreferenceChanged(PrefService* service,
                           std::string_view pref_name) override;

  raw_ptr<PrefService> pref_service_;  // Non-owning.

  // Non-owning pointers to the Registrars that have registered themselves
  // with us. We expect few registrars.
  base::ObserverList<Registrar>::Unchecked registrars_;
};

#endif  // CHROME_BROWSER_FONT_PREF_CHANGE_NOTIFIER_H_
