# Managing lifetime of a Profile

## KeyedService::Shutdown

If a `KeyedService` owns objects that depend on the lifetime of its `Profile` then it
needs to ensure that these objects are destroyed before the `Profile` is
destroyed (e.g. `content::WebContents` need to be be destroyed before their
`Profile` is destroyed).  This can be done by overriding
`KeyedService::Shutdown` - this method will be called before the `Profile` object
is destroyed.

## ScopedProfileKeepAlive

Typically, closing the last `Browser` window associated with a `Profile` will
start tearing down the `Profile`.  This is undesirable if there are
`content::WebContents` associated with that `Profile` in standalone (i.e.
non-`Browser`) windows that shouldn't go away without an explicit user action to
close such a window.  In such cases, `Profile` destruction can be postponed by
holding ` ScopedProfileKeepAlive`.

`ScopedProfileKeepAlive` is a strong reference to a `Profile` object.
It is very similar to `ScopedKeepAlive`, which is for the browser process.

## BrowserContextKeyedServiceShutdownNotifierFactory

If an object is not a `KeyedService`, but still needs to react to destruction of
a specific `KeyedService`, then it can do so using
`BrowserContextKeyedServiceShutdownNotifierFactory`.

For example, `extensions::ExtensionURLLoaderFactory` is owned by its remote mojo
client (i.e. it is not a `KeyedService`) but it still wants to avoid processing
further `chrome-extension://` resource requests after `Profile` destruction.
This is done by
[registering](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/extension_protocols.cc;l=954-959;drc=d632a0e263747345842d2a01f43179d711c4a5d4)
`ExtensionURLLoaderFactory::OnBrowserContextDestroyed` to be called
[when extension-related keyed services get
destroyed](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/extension_protocols.cc;l=1008-1009;drc=d632a0e263747345842d2a01f43179d711c4a5d4).

## Only when necessary: ProfileObserver::OnProfileWillBeDestroyed

If you *really* need to directly listen for Profile destruction you can use
`ProfileObserver::OnProfileWillBeDestroyed`, but in general if you need this,
it's a bad sign, as it means that you're using a feature that is not properly
encapsulated in a keyed service.
