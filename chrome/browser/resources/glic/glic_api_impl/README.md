## Glic web client API implementation

This directory contains the implementation of `../glic_api`. This code is
bundled into a single `.js` file and injected into the web client at startup,
where it communicates directly with the browser via Mojo.

The core web client API implementation (`GlicHostRegistryImpl` and
`GlicBrowserHostImpl`) lives in `glic_api_client.ts`, with feature-specific
delegates in subdirectories:

### actor/

Contains the client implementation for actor-related Glic API functions.

### annotation/

Contains the client implementation for scroll-to and highlight annotation
functions.

### experimental_triggering/

Contains the client implementation for experimental triggering Glic API
functions.

### geic/

Contains the client implementation for Gemini Enterprise in Chrome (GEIC) API
functions.

### skills/

Contains the client implementation for skills-related Glic API functions.

### tools/

Contains the client implementation for Chrome tool execution functions.

### zero_state_suggestions/

Contains the client implementation for zero-state suggestions Glic API
functions.

