# AI Overlay Dialog

This directory contains the WebUI implementation of the AI Overlay Dialog.

## Architecture

```
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
                  └─────┬───────────────────────┬─┘     └──────────────────┘
                        │                       │
                        │                       │
                        │                       │
           ┌────────────▼────────┐            ┌─▼──────────────────────┐
           │                     │            │                        │
           │ PageContextManager  │            │       ToolExecutor     │
           │                     │            │                        │
           └──────────────▲──────┘            └────────┬──▲────────────┘
                          │                            │  │
──────────────────────────┼────────────────────────────┼──┼─────────────────
                          │                            │  │
 Chrome C++ Controller    │                            │  │
                          │                            │  │
           ┌──────────────┴─────┐            ┌─────────▼──┴────────────┐
           │                    │            │                         │
           │ PageContextMonitor │            │     AiOverlayTools      │
           │                    │            │                         │
           └────────────────────┘            └─────────────────────────┘
```

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
- **`PageContextManager`**: Manages the context from the current page. Receives
    updates from the C++ side PageContextMonitor which listens to Browser-side
    changes like tab switches, navigations, etc.
- **`ToolExecutor`**: Responsible for unpacking tool calls from the model and
    sending them to the AiOverlayTools object in C++ to actually execute on the
    tool calls.

## State

Conversation is the source of truth for the conversation state which can be in one of three states:

* STOPPED (initial and ended) - In this state input/output is disabled and the
  server connection is torn down.
* LISTENING - The conversation is waiting for the user to issue a query
* TALKING - The conversation is playing back a response from the server

The UI in app.ts receives changes in the Conversation state but uses it's own,
similar but distinct, state machine to update the UI.

## Tools

Tool calls are defined in the [tools.mojom](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.mojom;l=1?q=tools.mojom&sq=&ss=chromium%2Fchromium%2Fsrc)
interface between ToolExecutor in WebUI and AIOverlayTools in C++. The
[generate_tool_definitions.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ai_overlay_dialog/tools/generate_tool_definitions.py)
build step reads the mojom and documentation in comments and converts it into
tool definition schemas passed to the model API.

## System Instructions

The personality and behavior of the model are defined by "System Instructions."
These instructions use a bespoke templating syntax that allows you to inject
dynamic information about the user's current web page and browser state.

Processing works by applying each step to the whole document. The order is:
conditionals, then numbering, then variable substitution.

### Variable Substitution

You can insert live information into your instructions using `${variable_name}`.

**Example:**
> The current page is "${title}" at ${url}.

### Conditionals (If/Then/Else)

You can include or exclude text based on whether a variable evaluates to a
truthy value.

*   **Full Format:** `?${variable}[Text if true]{else}[Text if false]{}?`
*   **Short Format:** `?${variable}[Text if true]{}?` (Shows nothing if false)

**Example:**
> ?${isLoaded}[Currently on ${title}.]{else}[The page is still loading.]{}?

Nesting conditionals is not supported.

### Automatic Numbering

When writing a list of instructions or tools, you can use `#{1}` to have the
numbers generated automatically. Each time you use `#{1}` in your template, it
will be replaced by the next number in sequence (1, 2, 3, etc.).

If you need multiple independent lists, you can use a different group number
(like `#{2}`).

**Example:**
> #{1}. Foo.
> #{1}. Bar.
>   #{2}. Reticulating
>   #{2}. Splines
> #{1}. Baz.

*Renders as:*
> 1. Foo.
> 2. Bar.
>   1. Reticulating
>   2. Splines
> 3. Use the page content.

## Dev

Provide an API key by starting chrome with chrome:
`--enable-features=AiOverlayDialog:api_key/<YOUR API KEY>`

Provide a .wav file to play (via button in UI) if you have no microphone ('+' is
replaced with '/' to allow specifying paths since slashes are used in
--enable-features syntax):
`--enable-features=AiOverlayDialog:mock_audio_path/+home+bokan+sounds+hello.wav`

You can provide both; feature params are also separated with '/'. E.g.

`--enable-features=AiOverlayDialog:api_key/123/mock_audio_path/+hello.wav`

Provide a path to the bundle of resources using:

`--ttc-bundle-url=https://example.com/foo`

Resources will be fetched from the given path at the specified URL.
