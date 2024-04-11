This directory contains the implementation of the Chromium UI. There are
subdirectories with implementations for specific toolkits and OSes.

This directory is often referred to in conversation as "cbui" or "c/b/ui",
pronounced "sea bee you eye".

Important subdirectories:
* views - the Views implementation of the UI, used on Windows, Mac, Linux, and
          ChromeOS/Lacros. This includes features like Omnibox, downloads.
* cocoa - the remaining Cocoa UI, used only on Mac. This directory used to
          contain a separate full implementation of the UI, parallel to the
          Views implementation.
* android - part of the Android implementation of the UI. See also
            //chrome/browser/android.
* webui - the WebUI parts of the browser UI. This includes things like the
          chrome://settings page and other WebUI pages.

Historically, the goal of this directory was to be platform agnostic, with
platform-specific logic in the above sub-directories. This didn't work and
attempting to maintain this structure was causing more problems than it was
solving, so we've removed this requirement.

In the event that a feature does need platform-specific implementations, use the
following structure:

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
