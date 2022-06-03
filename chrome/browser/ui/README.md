This directory contains the implementation of the Chromium UI. Code in the root
of this directory is toolkit- and platform-independent. There are subdirectories
with implementations for specific toolkits and OSes. Code in the root of this
directory should *not* be aware of platform-specific implementation details or
reach into the platform implementation subdirectories. This directory also
should not contain browser-process-scoped items that are not UI-specific, such
as parts of the startup logic; these sorts of things belong elsewhere in
chrome/browser/.

This directory is often referred to in conversation as "cbui" or "c/b/ui",
pronounced "sea bee you eye".

Important subdirectories:
* views - the Views implementation of the UI, used on Windows, Mac, Linux, and
          ChromeOS. This includes things like the browser window itself, tabs,
          dialogs, etc.
* cocoa - the remaining Cocoa UI, used only on Mac. This directory used to
          contain a separate full implementation of the UI, parallel to the
          Views implementation.
* android - part of the Android implementation of the UI. See also
            //chrome/browser/android.
* webui - the WebUI parts of the browser UI. This includes things like the
          chrome://settings page and other WebUI pages.

A common pattern is for code in //chrome/browser/ui to define a
platform-independent interface which then has implementations in
//chrome/browser/ui/views and //chrome/browser/ui/android. This pattern is often
followed even for features that don't exist on Android, in which case the
Android implementation is often a stub.

This pattern often looks like this:

[//chrome/browser/ui/browser_dialogs.h](browser_dialogs.h):  
    void ShowMyDialog(...);

//chrome/browser/ui/views/my_dialog_views.cc:  
    void ShowMyDialog(...) { ... }

//chrome/browser/ui/android/my_dialog_android.cc:  
    void ShowMyDialog(...) { ... }

Because "Chromium UI" is such a large surface area, do not add new files
directly to this directory; instead, add subdirectories with more specific
OWNERS and place new features and files in them. Cleanup of existing scattered
files is also welcome.
