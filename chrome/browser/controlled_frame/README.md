# Controlled Frame Browser Implementation (`chrome/browser/controlled_frame`)

This directory contains the browser-process implementation of the
[Controlled Frame API](https://github.com/WICG/controlled-frame), which enables
Isolated Web Apps (IWAs) and other isolated contexts to embed and control
arbitrary web content via the `<controlledframe>` element.

## Companion Documentation

- **Common Controlled Frame Schemas & Availability:**
  [`//chrome/common/controlled_frame`](/chrome/common/controlled_frame/README.md)
- **Renderer Controlled Frame Bindings:**
  [`//chrome/renderer/controlled_frame`](/chrome/renderer/controlled_frame/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Key Components

- `api/controlled_frame_internal_api.{h,cc}`: Implements internal extension
  functions backing `<controlledframe>` context menus and webView internal
  methods without exposing Chrome Extensions APIs to the embedding IWA.
- `ControlledFrameExtensionsBrowserAPIProvider`: Implements
  `extensions::ExtensionsBrowserAPIProvider` (registered on
  `ExtensionsBrowserClient`) to register Controlled Frame-specific extension
  functions (`ControlledFrameInternal*`) into `ExtensionFunctionRegistry`.
- `ControlledFrameMediaAccessHandler`, `ControlledFrameMediaPermissionCache`, &
  `ControlledFrameMediaPermissionCacheFactory`: Mediates and caches media
  permission requests (`microphone`, `camera`) originating inside a
  `<controlledframe>` guest, delegating permission checks to the embedding IWA
  origin.
- `ControlledFrameMenuIconLoader`: Loads custom context menu icons specified by
  the `<controlledframe>` `contextMenus` API.
- `controlled_frame_user_agent_util.{h,cc}`: Computes the default `User-Agent`
  override (`GetDefaultControlledFrameUserAgentOverride`) for
  `<controlledframe>` instances.
