# AI Overlay Dialog

This directory contains the WebUI implementation of the AI Overlay Dialog.

## Architecture

                ┌──────────────────┐ ┌─────────────┐
                │   AudioCapturer  │ │ AudioPlayer │
                └─────────▲────────┘ └───────▲─────┘
                          │                  │
                          │                  │
                          │                  │
                          │                  │
                          │                  │
                  ┌───────┴──────────────────┴────┐
                  │                               │
                  │     AIOverlayDialogElement    │
  UI AND IO       │                               │
                  └───────────────┬───────────────┘
                                  │
──────────────────────────────────┼──────────────────────────────────────────
                                  │
                                  │
  LOGIC                           │
                  ┌───────────────▼───────────────┐     ┌──────────────────┐
                  │                               │     │                  │
                  │          Conversation         ┼─────►    ApiSession    │
                  │                               │     │                  │
                  └───────────────────────────────┘     └──────────────────┘

(diagram can be modified using asciiflow.com)

- **`AiOverlayDialogElement` (`ai_overlay_dialog.ts`)**: The main custom element
    (`<ai-overlay-dialog>`) and entry point. It instantiates a Conversation and
    provides the audio I/O components to it.
- **`Conversation` (`conversation.ts`)**: The central coordinator class that
    bridges the UI/IO with the API session and implements the core logic and
    state tracking. This is a long lived object that persists across page loads
    and tabs.
- **`ApiSession` (`api_session.ts`)**: Manages the `WebSocket` connection to the
    AI backend and handles communication. This object is recreated whenever page
    context is updated. In practice, this means whenever the active tab changes
    or is navigated.
- **`Persona` (`persona.ts`)**: Defines the system instruction for the AI.

## State

Conversation is the source of truth for the conversation state which can be in one of three states:

* STOPPED (initial and ended) - In this state input/output is disabled and the
  server connection is torn down.
* LISTENING - The conversation is waiting for the user to issue a query
* TALKING - The conversation is playing back a response from the server

The UI in app.ts receives changes in the Conversation state but uses it's own,
similar but distinct, state machine to update the UI.

## Dev

Provide an API key by starting chrome with chrome:
`--enable-features=AiOverlayDialog:api_key/<YOUR API KEY>`

Provide a .wav file to play (via button in UI) if you have no microphone ('+' is
replaced with '/' to allow specifying paths since slashes are used in
--enable-features syntax):
`--enable-features=AiOverlayDialog:mock_audio_path/+home+bokan+sounds+hello.wav`

You can provide both; feature params are also separated with '/'. E.g.

`--enable-features=AiOverlayDialog:api_key/123/mock_audio_path/+hello.wav`
