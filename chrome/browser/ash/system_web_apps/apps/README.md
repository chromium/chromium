This directory contains configuration for web applications that come
pre-installed on ChromeOS. This includes:
  * System web applications (e.g. files app). The logic primarily comes in the
    form of ash::SystemWebAppDelegate subclasses which provide configuration
    such as minimum window size, whether to show a tab strip, whether to show
    the app in the launcher.
  * Other UI surfaces powered by by WebUI (e.g. Crosh). The logic primarily
    comes in the form of content::WebUIConfig subclasses. This is needed to
    provide static entry points. This logic may also generate some dynamic data
    to pass to the WebUI.
