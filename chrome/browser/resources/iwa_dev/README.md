# Isolated Web Apps Developer WebUI Resources (`chrome/browser/resources/iwa_dev`)

This directory contains the TypeScript, HTML, and CSS frontend resources for the
Isolated Web Apps (IWA) Developer UI (`chrome://iwa-dev`), built with Lit 3.0.

## Companion Documentation

- **Backend WebUI & Mojo Handler:**
  [`//chrome/browser/ui/webui/iwa_dev`](/chrome/browser/ui/webui/iwa_dev/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Key Frontend Components

- `app.ts` (`IwaDevAppElement` / `<iwa-dev-app>`): Root Lit component
  coordinating the list of installed dev-mode IWAs, installation/update dialogs,
  per-app update options persisted in `localStorage`, and Mojo IPC with
  `IwaDevPageHandler` via the auto-generated `browserProxyFactory`
  (`iwa_dev.mojom-webui.ts`).
- `install_dialog.ts` (`<iwa-dev-install-dialog>`) & `install_tab.ts`
  (`IwaDevInstallTabElement`): Modal dialog and base tab component driving
  developer-mode IWA installation across three source tabs:
  - `install_dev_proxy_tab.ts` (`<iwa-dev-install-dev-proxy-tab>`): Installs an
    IWA from a dev proxy URL.
  - `install_local_bundle_tab.ts` (`<iwa-dev-install-local-bundle-tab>`):
    Prompts for and installs a local `.swbn` Signed Web Bundle file.
  - `install_update_manifest_tab.ts` (`<iwa-dev-install-update-manifest-tab>`):
    Fetches an Update Manifest URL and lets the developer select an update
    channel and version to install.
- `installed_app_list_item.ts` (`<installed-app-list-item>`): Displays an
  installed dev-mode IWA entry with source details and actions to launch,
  trigger an update, configure update options, or uninstall.
- `update_options_dialog.ts` (`<iwa-dev-update-options-dialog>`): Dialog for
  manifest-installed dev-mode IWAs to configure the target update channel,
  pinned version, and `allowDowngrades` option.
- `combobox.ts` (`<iwa-dev-combobox>`): Accessible combobox element used for
  channel and version selection.
