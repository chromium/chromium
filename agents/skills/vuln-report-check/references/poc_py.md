# Python server PoC (`server.py`)

Automated reproduction copies `server.py`, every other attachment, and a `gen`
link to the build into one folder, then runs `python3 server.py` (Python 3.11,
no arguments), waits up to 17 s, then requests the attached `.html` page,
`/poc.html`, or `/index.html` on `http://localhost:8000` and needs HTTP `200`
with `text/html`.

## Settling `unknown`

### `py.server_contract`

Confirm that `GET` on the entry page over plain HTTP on the stated port returns
`200` with `text/html`. A directory listing on `/` does not count. Scripts that
read arguments (`sys.argv`, `argparse`) stay `unknown`: `pass` only if the
server runs with no arguments (optional arguments that fall back to port 8000
are fine); required arguments `fail`. Prefer offering `--smoke` over guessing.

### `supported_apis`

The server may only send responses and headers and coordinate timing. `fail` if
it changes browser profiles, modifies `./gen`, or calls methods that do not
exist.

### `py.no_logging`

Routes like `/log` or `/report` fail only if they print, store, or send out
request data. Returning `204` or tracking request order in memory is fine.

### `py.no_user_interaction`

`fail` on manual clicks or key presses, permission prompts,
`navigator.userActivation`, or a required `location.hash` / `search`. A scripted
`element.click()` on load is fine.

### `py.environment_independent`

`fail` on `Host` checks that reject `localhost:8000` or `127.0.0.1`, hash or
length checks on `poc.html`, TLS certificates expiring within 7 days, or
external helper processes.

## Do not flag

- Minimal static servers that only give an attached page an `http:` origin.
- Custom headers (CSP, COOP/COEP, `Permissions-Policy`, `Clear-Site-Data`,
  `Cache-Control`, and so on) and generated responses (HTML, workers, WASM,
  images).
- Race coordination: delays, barriers, chunked flushes, redirects.
