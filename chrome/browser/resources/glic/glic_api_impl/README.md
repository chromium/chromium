## Glic web client API implementation

This directory contains the implementation of ../glic_api.

### glic_api_client.ts and glic_api_injected_client.ts

Contains the web client side of the postMessage interface. Directly implements
objects used by the web client. This code is bundled into a .js file and sent to
the web client at startup for execution.

### glic_api_host.ts

Contains the browser-side of the postMessage interface, which runs within a
WebUI. Forwards requests from the web client to the browser via mojo.

### request_types.ts

The private postMessage API between the web client and WebUI.
