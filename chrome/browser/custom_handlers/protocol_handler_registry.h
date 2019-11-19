// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// This is where handlers for protocols registered with
// navigator.registerProtocolHandler() are registered. Each Profile owns an
// instance of this class, which is initialized on browser start through
// Profile::InitRegisteredProtocolHandlers(), and they should be the only
// instances of this class.
class ProtocolHandlerRegistry : public KeyedService {
 public:
  enum HandlerSource {
    USER,    // The handler was installed by user
    POLICY,  // The handler was installed by policy
  };

  typedef std::map<std::string, ProtocolHandler> ProtocolHandlerMap;
  typedef std::vector<ProtocolHandler> ProtocolHandlerList;
  typedef std::map<std::string, ProtocolHandlerList> ProtocolHandlerMultiMap;

  // |Delegate| provides an interface for interacting asynchronously
  // with the underlying OS for the purposes of registering Chrome
  // as the default handler for specific protocols.
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void RegisterExternalHandler(const std::string& protocol);
    virtual void DeregisterExternalHandler(const std::string& protocol);
    virtual bool IsExternalHandlerRegistered(const std::string& protocol);
    virtual void RegisterWithOSAsDefaultClient(
        const std::string& protocol,
        shell_integration::DefaultWebClientWorkerCallback callback);
    virtual void CheckDefaultClientWithOS(
        const std::string& protocol,
        shell_integration::DefaultWebClientWorkerCallback callback);
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnProtocolHandlerRegistryChanged() = 0;
  };

  // Creates a new instance.
  ProtocolHandlerRegistry(content::BrowserContext* context,
                          std::unique_ptr<Delegate> delegate);
  ~ProtocolHandlerRegistry() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when a site tries to register as a protocol handler. If the request
  // can be handled silently by the registry - either to ignore the request
  // or to update an existing handler - the request will succeed. If this
  // function returns false the user needs to be prompted for confirmation.
  bool SilentlyHandleRegisterHandlerRequest(const ProtocolHandler& handler);

  // Called when the user accepts the registration of a given protocol handler.
  void OnAcceptRegisterProtocolHandler(const ProtocolHandler& handler);

  // Called when the user denies the registration of a given protocol handler.
  void OnDenyRegisterProtocolHandler(const ProtocolHandler& handler);

  // Called when the user indicates that they don't want to be asked about the
  // given protocol handler again.
  void OnIgnoreRegisterProtocolHandler(const ProtocolHandler& handler);

  // Removes all handlers that have the same origin and protocol as the given
  // one and installs the given handler. Returns true if any protocol handlers
  // were replaced.
  bool AttemptReplace(const ProtocolHandler& handler);

  // Returns a list of protocol handlers that can be replaced by the given
  // handler.
  ProtocolHandlerList GetReplacedHandlers(const ProtocolHandler& handler) const;

  // Clears the default for the provided protocol.
  void ClearDefault(const std::string& scheme);

  // Returns true if this handler is the default handler for its protocol.
  bool IsDefault(const ProtocolHandler& handler) const;

  // Initializes default protocol settings and loads them from prefs.
  // This method must be called to complete initialization of the
  // registry after creation, and prior to use.
  void InitProtocolSettings();

  // Returns the offset in the list of handlers for a protocol of the default
  // handler for that protocol.
  int GetHandlerIndex(const std::string& scheme) const;

  // Get the list of protocol handlers for the given scheme.
  ProtocolHandlerList GetHandlersFor(const std::string& scheme) const;

  // Get a list of protocol handlers registered in [begin, end).
  // Does not include predefined or policy installed handlers.
  ProtocolHandlerList GetUserDefinedHandlers(base::Time begin,
                                             base::Time end) const;

  // Clear all protocol handlers registered in [begin, end).
  // Does not delete predefined or policy installed handlers.
  void ClearUserDefinedHandlers(base::Time begin, base::Time end);

  // Get the list of ignored protocol handlers.
  ProtocolHandlerList GetIgnoredHandlers();

  // Yields a list of the protocols that have handlers registered in this
  // registry.
  void GetRegisteredProtocols(std::vector<std::string>* output) const;

  // Returns true if we allow websites to register handlers for the given
  // scheme.
  bool CanSchemeBeOverridden(const std::string& scheme) const;

  // Returns true if an identical protocol handler has already been registered.
  bool IsRegistered(const ProtocolHandler& handler) const;

  // Returns true if an identical protocol handler has already been registered
  // by the user.
  bool IsRegisteredByUser(const ProtocolHandler& handler);

  // Returns true if the scheme has at least one handler that is registered by
  // policy.
  bool HasPolicyRegisteredHandler(const std::string& scheme);

  // Returns true if an identical protocol handler is being ignored.
  bool IsIgnored(const ProtocolHandler& handler) const;

  // Returns true if an equivalent protocol handler has already been registered.
  bool HasRegisteredEquivalent(const ProtocolHandler& handler) const;

  // Returns true if an equivalent protocol handler is being ignored.
  bool HasIgnoredEquivalent(const ProtocolHandler& handler) const;

  // Causes the given protocol handler to not be ignored anymore.
  void RemoveIgnoredHandler(const ProtocolHandler& handler);

  // Returns true if the protocol has a default protocol handler.
  bool IsHandledProtocol(const std::string& scheme) const;

  // Removes the given protocol handler from the registry.
  void RemoveHandler(const ProtocolHandler& handler);

  // Remove the default handler for the given protocol.
  void RemoveDefaultHandler(const std::string& scheme);

  // Returns the default handler for this protocol, or an empty handler if none
  // exists.
  const ProtocolHandler& GetHandlerFor(const std::string& scheme) const;

  // Returns a translated URL if |url| is handled by a protocol handler,
  // otherwise it returns an empty URL.
  GURL Translate(const GURL& url) const;

  // Puts this registry in the enabled state - registered protocol handlers
  // will handle requests.
  void Enable();

  // Puts this registry in the disabled state - registered protocol handlers
  // will not handle requests.
  void Disable();

  // This is called by the UI thread when the system is shutting down. This
  // does finalization which must be done on the UI thread.
  void Shutdown() override;

  // Registers the preferences that we store registered protocol handlers in.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  bool enabled() const { return enabled_; }

  // Add a predefined protocol handler. This has to be called before the first
  // load command was issued, otherwise the command will be ignored.
  void AddPredefinedHandler(const ProtocolHandler& handler);

 private:
  friend class base::DeleteHelper<ProtocolHandlerRegistry>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;

  // for access to InstallDefaultsForChromeOS
  friend class ProtocolHandlerRegistryFactory;

  friend class ProtocolHandlerRegistryTest;
  friend class RegisterProtocolHandlerBrowserTest;

  // Puts the given handler at the top of the list of handlers for its
  // protocol.
  void PromoteHandler(const ProtocolHandler& handler);

  // Saves a user's registered protocol handlers.
  void Save();

  // Returns a pointer to the list of handlers registered for the given scheme,
  // or NULL if there are none.
  const ProtocolHandlerList* GetHandlerList(const std::string& scheme) const;

  // Install default protocol handlers for chromeos which must be done
  // prior to calling InitProtocolSettings.
  void InstallDefaultsForChromeOS();

  // Makes this ProtocolHandler the default handler for its protocol.
  void SetDefault(const ProtocolHandler& handler);

  // Insert the given ProtocolHandler into the registry.
  void InsertHandler(const ProtocolHandler& handler);

  // Returns a JSON list of protocol handlers. The caller is responsible for
  // deleting this Value.
  base::Value* EncodeRegisteredHandlers();

  // Returns a JSON list of ignored protocol handlers. The caller is
  // responsible for deleting this Value.
  base::Value* EncodeIgnoredHandlers();

  // Notifies observers of a change to the registry.
  void NotifyChanged();

  // Registers a new protocol handler.
  bool RegisterProtocolHandler(const ProtocolHandler& handler,
                               const HandlerSource source);

  // Registers protocol handlers from the preference.
  void RegisterProtocolHandlersFromPref(const char* pref_name,
                                        const HandlerSource source);

  // Get all handlers with a timestamp in [begin,end) from |handlers|.
  ProtocolHandlerList GetHandlersBetween(
      const ProtocolHandlerMultiMap& handlers,
      base::Time begin,
      base::Time end) const;

  // Get all ignored handlers with a timestamp in [begin,end).
  ProtocolHandlerList GetUserIgnoredHandlers(base::Time begin,
                                             base::Time end) const;

  // Get the DictionaryValues stored under the given pref name that are valid
  // ProtocolHandler values.
  std::vector<const base::DictionaryValue*> GetHandlersFromPref(
      const char* pref_name) const;

  // Ignores future requests to register the given protocol handler.
  void IgnoreProtocolHandler(const ProtocolHandler& handler,
                             const HandlerSource source);

  // Ignores protocol handlers from the preference.
  void IgnoreProtocolHandlersFromPref(const char* pref_name,
                                      const HandlerSource source);

  // Verifies if the handler exists in the map.
  bool HandlerExists(const ProtocolHandler& handler,
                     ProtocolHandlerMultiMap* map);

  // Verifies if the handler exists in the list.
  bool HandlerExists(const ProtocolHandler& handler,
                     const ProtocolHandlerList& list);

  // Erases the handler that is guaranteed to exist from the map.
  void EraseHandler(const ProtocolHandler& handler,
                    ProtocolHandlerMultiMap* map);

  // Erases the handler that is guaranteed to exist from the list.
  void EraseHandler(const ProtocolHandler& handler, ProtocolHandlerList* list);

  // Called with the default state when the default protocol client worker is
  // done.
  void OnSetAsDefaultProtocolClientFinished(
      const std::string& protocol,
      shell_integration::DefaultWebClientState state);

  // Gets the callback for DefaultProtocolClientWorker.
  shell_integration::DefaultWebClientWorkerCallback GetDefaultWebClientCallback(
      const std::string& protocol);

  // Map from protocols (strings) to protocol handlers.
  ProtocolHandlerMultiMap protocol_handlers_;

  // Protocol handlers that the user has told us to ignore.
  ProtocolHandlerList ignored_protocol_handlers_;

  // These maps track the source of protocol handler registrations for the
  // purposes of disallowing the removal of handlers that are registered by
  // policy. Every entry in protocol_handlers_ should exist in at least one of
  // the user or policy maps.
  ProtocolHandlerMultiMap user_protocol_handlers_;
  ProtocolHandlerMultiMap policy_protocol_handlers_;

  // These lists track the source of protocol handlers that were ignored, for
  // the purposes of disallowing the removal of handlers that are ignored by
  // policy. Every entry in ignored_protocol_handlers_ should exist in at least
  // one of the user or policy lists.
  ProtocolHandlerList user_ignored_protocol_handlers_;
  ProtocolHandlerList policy_ignored_protocol_handlers_;

  // A list of handlers that were preinstalled.
  ProtocolHandlerList predefined_protocol_handlers_;

  // Protocol handlers that are the defaults for a given protocol.
  ProtocolHandlerMap default_handlers_;

  // The browser context that owns this ProtocolHandlerRegistry.
  content::BrowserContext* context_;

  // The Delegate that registers / deregisters external handlers on our behalf.
  std::unique_ptr<Delegate> delegate_;

  // If false then registered protocol handlers will not be used to handle
  // requests.
  bool enabled_;

  // Whether or not we are loading.
  bool is_loading_;

  // When the table gets loaded this flag will be set and any further calls to
  // AddPredefinedHandler will be rejected.
  bool is_loaded_;

  base::ObserverList<Observer> observers_;

  // Makes it possible to invalidate the callback for the
  // DefaultProtocolClientWorker.
  base::WeakPtrFactory<ProtocolHandlerRegistry> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerRegistry);
};
#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_
