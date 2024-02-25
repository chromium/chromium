## App Shims

### What are App Shims?

App shims are thin helper applications, created by Chrome, that enable [web apps](/docs/webapps/README.md) to show up as applications separate from Chrome on macOS. App shims largely don't contain code of their own, but merely load `Google Chrome Framework.framework` and run all the code from there. This directory contains the chrome code that only runs in the app shim process.

### App Shim lifetime

While app shims can be launched by Chrome, by the OS or by the User directly, in all cases execution roughly follows the same three phases:

1) *Early startup*: In this phase, the code in `app_mode_loader_mac.mm` is responsible for figuring out what version of Chrome this app shim is associated with, and dlopen the framework for that version. If loading the chrome framework fails for some reason, the app shim will instead try launching chrome with the `--app-id` command line flag before terminating itself.

2) *Bootstrap*: The entry-point for code in this phase is in `chrome_main_app_mode_mac.mm`. The main thing that happens during this phase is that the app shim either looks for an already running instance of (the right version and `user_data_dir`) of Chrome, or if none is found launches a new Chrome instance. Once a running Chrome is found, the app shim initiates a mojo connection to Chrome, passing along any launch-relevant information.

3) *Running*: Once the initial mojo connection has been established with Chrome, a reply is received from Chrome and initialization of the App Shims is completed. Depending on the type of launch (and the type of application the app shim represents) the app shim might self-terminate at this point, or it can start acting as a remote cocoa host, displaying any windows for the app the shim represents.

### `base::Feature` and field trial support

Normal Chrome helper processes (such as renderer and utility processes) get their feature and field trial state passed in by the browser process on launch. Unfortunately because app shims are not always launched by Chrome, the same isn't quite possible for app shims. This means that some care needs to be taken when using features in field trials in app shims:

#### Early startup
In the *early startup phase*, no support for features and field trials is possible. This code should be as minimal as possible with minimal dependencies on other Chrome code, so this shouldn't be too much of a limitation.

#### Bootstrap
In the *bootstrap phase*, a lot more code runs. And this includes Chrome components such as mojo that depend on `base::Feature`. When the app shim was launched by Chrome this is no problem, as Chrome will make sure to pass the full feature and field trial state over the command line when launching an app shim. On the other hand when the user or OS launched an app shim, this state is not available on the command line. In that case, the app shim will instead load feature and field trial state from a `ChromeFeatureState` file in the `user_data_dir`. Every time Chrome starts this file is updated with the current feature and field trial state (for testing/development purposes the command line can be used to override feature state as read from the file; these overrides will also be forwarded to Chrome, if the app shim ends up launching Chrome rather than connecting to an existing Chrome instance).

If Chrome wasn't currently running when an app shim is launched, it is possible for the feature state to change when Chrome is launched. This would result in Chrome and the app shim having a different idea of what the state of various features is. This is unavoidable at this phase of app shim execution, so to make sure the consequences of this are thought about, `AppShimController` uses `base::FeatureList::SetEarlyAccessInstance` with an explicit allow-list of feature names that can be used in the *bootstrap phase*. Any attempt to check the state of a `base::Feature` not on this allow-list will behave the same as if no `base::FeatureList` instance was set at all, i.e. CHECK-fail.

#### Running
Finally at the start of the *running phase*, Chrome passes its actual current feature state to the app shim via mojo. If Chrome originally launched the app shim this would be redundant, but passing it anyway means app shims don't have to have separate code paths depending on they are launched. At this point `base::FeatureList` and `base::FieldTrialList` are recreated with this new state, after which app shims should behave more or less the same as any other helper process as far as feature and field trial state is concerned.

### Launch and shim creation

The code responsible for creating and launching app shims can be found in [/chrome/browser/web_applications/os_integration/web_app_shortcut_mac.mm](/chrome/browser/web_applications/os_integration/web_app_shortcut_mac.mm).
