# ChromeOS Settings — Manage Isolated Web Apps Subpage (`chrome/browser/resources/ash/settings/os_apps_page/manage_isolated_web_apps_page`)

This directory implements the ChromeOS Settings (`chrome://os-settings`) subpage
for managing Isolated Web Apps (IWAs) under the Apps section.

## Companion Documentation

- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Key Components

- `manage_isolated_web_apps_subpage.ts` & `.html`: Polymer element
  (`<settings-manage-isolated-web-apps-subpage>`) displaying a description link
  and toggle (`<settings-toggle-button>`) to enable or disable Isolated Web Apps
  on ChromeOS (combining `ash.isolated_web_apps_enabled` and
  `profile.isolated_web_app.install.user_install_enabled`, including
  policy-enforced states).
