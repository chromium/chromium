
# System Web Apps


## Background
The SWA platform is meant to support the development of all system and highly privileged applications for ChromeOS. An example would be the OS Settings app, which has access to additional APIs and features than a normal Web App. 

The SWA platform grew out of two desires. First, to deprecate Chrome Apps. They had proven themselves to be a good first attempt at an ecosystem for Chrome development, but didn’t gain traction. Many system level applications were built on the Chrome Apps platform.

Second, today the Web Platform is a much more powerful platform that it was when Chrome Apps were introduced, making it a suitable and more maintainable alternative to Chrome Apps. The desire was to have a replacement for Chrome Apps be as close to standard web applications as possible.

## Overview of a SWA
A SWA is simply a Web Application, but it has access to more APIs and is distributed differently. The intent is to provide an extended platform of Web Apps to make them suitable for sensitive and internal applications.
We have a sample system web application to test out new platform features as well as to demonstrate how to build a basic SWA. For reference see: [the code](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/system_web_apps/apps/sample_system_web_app_info.h) and [the initial checkin](https://chromium-review.googlesource.com/c/chromium/src/+/1956978)

## Distribution
System Web Apps are distributed with the OS image. For most apps, icons, js and html are all packaged with GRIT. C++ code is compiled into the OS. This means that there is no versioning mismatch possible between ChromeOS and an SWA, or from the C++ part of an SWA to the web part. This distribution also implies that releases and bug fixes are tied to the ChromeOS release schedule.
Most SWAs are built in the Chromium source tree, but it is possible to develop in Google3, and be distributed with ChromeOS. The basics are that the google3 code and resources are compiled and packaged. They’re brought into gclient for builds containing src_internal.

## Installation
System Web Apps are installed on startup using the Web Apps installation pipeline. The key differences are:
* SWAs don’t use the html->manifest path at all. They are installed directly by creating a WebAppInstallInfo object directly in C++ code, and skipping much of the installation process.
* The SWAs are checked for installation or upgrade on boot, and re-installed up to the current version of ChromeOS.

## Serving Resources
Serving of SWA resources is entirely local to the device. HTML, JS, icons and other resources are usually distributed as part of the .pak files, and are requested and served using resource ids. It is also possible to load from files on disk in the case of large resources. SWAs still have network access, and can download data that’s not part of their core executable.

## Launching
Launching a System Web App reuses the normal Web App launching. On clicking the icon, an app window is opened with the address of the SWA. This is done mostly through the normal means, but there are some subtle [differences](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/ui/web_applications/system_web_app_ui_utils.cc;l=145). Key takeaways:
* Usually SWAs are launched from their icon in the app launcher.
* Apps can add other launch surfaces for their apps e.g. a button in another piece of UI, or in another app. To do this, call the LaunchSystemWebAppAsync function from C++.
* Apps can choose to hide their icon from the launcher.
* A SWA can register it's intent to capture urls, any urls in the SWAs scope will open in the app.

## UI
The windowing system is re-used from the Web App Platform, allowing the same flexibility as a normal web application. Some takeaways:
* Single and multi window apps are possible.
* Apps can embed their content in other surfaces, like modal dialogs.
* We don’t show the origin like a normal web application.
* SWA developers should consider disabling some hotkey combinations, as some can be confusing for users. (for example, opening the file dialog from settings)

## Offline
Many System Web Apps can run offline natively. Because core resources are packaged with the OS release, no network access will be needed to launch the app. Apps can of course load network resources, but offline is possible if no loads are needed.

## APIs
SWAs have a few ways of accessing Chrome APIs. 
All normal web platform APIs are available like a vanilla web application.
Pre-release web platform APIs are available with permission. It is expected that teams using these are working with the web platform team to ok the use and track it for versioning and support.
Custom APIs can be written by the SWA owner, and exposed through Mojo. These are to be used by SWAs that don’t process untrusted data.
Custom APIs can also be exposed through PostMessage. This method is to be used when an SWA processes untrusted data.

## Untrusted Data
Untrusted data, meaning content from untrustworthy sources (e.g network data, user data loaded from the disk), is ok to process in SWAs. It requires sandboxing into a frame in the chrome-untrusted:// scheme.
## Background Runtime
System web apps provide a timed background processing functionality, and Shared Workers. It's possible to communicate between windows or with the background task with Shared Workers. The background page is opened on a timer or at login.

## OS Integrations (lockscreen, shelf, shortcuts, etc.)
SWAs haven’t found a use case for integrating more deeply with the OS. The platform is ready to build out new launch capabilities or UI surfaces to run in the lock/login screen.
