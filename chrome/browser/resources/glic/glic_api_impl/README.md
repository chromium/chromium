## Glic web client API implementation

This directory contains the implementation of ../glic_api.

### client/

Contains the web client side of the postMessage interface. Directly implements
objects used by the web client. This code is bundled into a .js file and sent to
the web client at startup for execution.

### host/

Contains the browser-side of the postMessage interface, which runs within a
WebUI. Forwards requests from the web client to the browser via mojo.

### request_types.ts

The private postMessage API between the web client and WebUI.

### transport/

Shared utilities for message passing with postMessage.

### actor/

The actor module. Contains the client implementation for actor related glic API
functions.
  actor_client.ts - client implementation.

### zero_state_suggestions/

The zero state suggestions module. Contains the client implementation for zero
state suggestions related glic API functions.
  zero_state_suggestions_client.ts - client implementation.

