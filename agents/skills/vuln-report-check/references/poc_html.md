# HTML PoC (`poc.html`)

Without `server.py`, automated reproduction copies only the entry page (plus
`patch.diff`), opens it as `file:///tmp/poc.html` with no network, and links the
build's `gen/` folder next to it. No flags are added: Mojo PoCs need
`--enable-blink-features=MojoJS` in `flags`.

## Settling `unknown`

### Trigger runs on load (agent judges; no check ID)

The checker does not test this. Report a blocker under **Must fix**.

- Blocker if the trigger waits on an unreachable branch, a `postMessage` that
  never arrives, a manual DevTools call, or a timer longer than ~10 s. Fix: run
  the trigger on load.
- Fine if it runs from top-level script, `onload` / `DOMContentLoaded`,
  `requestAnimationFrame`, a short `setTimeout`, or pure HTML/CSS/SVG.

### `html.no_user_interaction`

- `fail` if it needs real user activation, browser dialogs, permission prompts,
  or manual clicks or key presses.
- `pass` for synthetic `.click()` / `dispatchEvent()` on load that needs no user
  activation.

### `supported_apis`

Any experimental Blink or web feature must be enabled by `flags` (for example
`--enable-experimental-web-platform-features` or `--enable-blink-features=...`).

## Do not flag

- Invalid HTML or JavaScript: the syntax error may be the bug itself.
- Inline `<script>` / `<style>`, `data:` and `Blob` URLs, `<iframe srcdoc>`.
- SVG / MathML `xmlns="http://www.w3.org/..."` URIs.
- Markup or CSS crashes with no script at all.
- Mojo use with the MojoJS flag in `flags`.
