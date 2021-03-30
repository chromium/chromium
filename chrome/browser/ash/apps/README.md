# Chrome OS Apps Platform

Chrome OS features multiple app platforms, such as ARC++ and desktop PWAs,
that are based on different underlying technologies. UX-wise, it is an
explicit goal to minimise the differences between them and give users the
feeling that all apps have as similar properties, management, and features as
possible.

This directory contains generic code to facilitate app platforms on Chrome OS
communicating with the browser and each other. This layer consists of
app-platform-agnostic code, which each app platform's custom implementation
then plugs into. Example features include:

* `ash/apps/intent_helper` - allows installed apps that handle particular
  URLs to be opened by users from the omnibox. For ARC++, custom code queries
  the ARC++ container; for desktop PWAs, data is contained in the browser
  itself.
