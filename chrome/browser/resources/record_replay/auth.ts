// external (c++-side) interface
declare function getEnv(key: string): string | null;
declare function getBuildId(): string;
declare function getReplayUserToken(): string | null;
declare function setReplayUserToken(token: string | null): void;
declare function getReplayRefreshToken(): string | null;
declare function setReplayRefreshToken(token: string | null): void;
declare function showAuthenticationError(message: string): void;
declare function openExternalBrowser(url: string): Promise<void>;
declare function onSignInButtonClicked(callback: () => void): void;

function getAuthHost() {
  return getEnv("RECORD_REPLAY_AUTH_HOST") || "webreplay.us.auth0.com";
}
function getAuthClientId() {
  return getEnv("RECORD_REPLAY_AUTH_CLIENT_ID") || "4FvFnJJW4XlnUyrXQF8zOLw6vNAH1MAo";
}
function getViewHost() {
  return getEnv("RECORD_REPLAY_VIEW_HOST") || "https://app.replay.io";
}
function getAPIServer() {
  return getEnv("RECORD_REPLAY_API_SERVER") || "https://api.replay.io/v1/graphql";
}
function getOriginalApiKey() {
  return getEnv("RECORD_REPLAY_API_KEY");
}
function getTelemetryUrl() {
  return "https://telemetry.replay.io";
}
function isTelemetryEnabled() {
  return true;
}

// https://developer.mozilla.org/en-US/docs/Glossary/Base64#the_unicode_problem
function base64URLEncode(bytes: Uint8Array): string {
  const binString = String.fromCodePoint(...bytes);
  const base64 = btoa(binString);
  return base64.replace(/\+/g, "-").replace(/\//g, "_").replace(/=/g, "");
}
function base64URLDecode(base64: string): Uint8Array {
  const binString = atob(base64.replace(/\-/g, "+").replace(/_/g, "/"));
  const bytes = (Uint8Array.from as any)(binString, (m: string) => m.codePointAt(0));
  return bytes;
}

function pingTelemetry(source: string, name: string, data: Record<string, any>) {
  const url = getTelemetryUrl();
  const enabled = isTelemetryEnabled();

  if (!enabled || !url) {
    return;
  }

  // fetch the user info to send in `Authorization` header.
  const auth = getOriginalApiKey() || getReplayUserToken();

  fetch(url, {
    method: 'POST',
    headers: auth ? { Authorization: `Bearer ${auth}` } : undefined,
    body: JSON.stringify({
      ...data,
      event: 'Gecko',
      build: getBuildId(),
      ts: Date.now(),
      source,
      name,
    })
  }).catch(console.error);
}

function tokenInfo(token: string): Record<string, string> | null {
  const [_header, encPayload, _cypher] = token.split(".", 3);
  if (typeof encPayload !== "string") {
    return null;
  }

  let payload;
  try {
    const decPayload = base64URLDecode(encPayload);
    payload = JSON.parse(new TextDecoder().decode(decPayload));
  } catch (err) {
    return null;
  }

  if (typeof payload !== "object") {
    return null;
  }

  return payload as Record<string, string>;
}

function tokenExpiration(token: string) {
  const userInfo = tokenInfo(token);
  if (!userInfo) {
    return null;
  }
  const exp = userInfo["exp"];
  return typeof exp === "number" ? exp * 1000 : null;
}

async function validateUserToken() {
  const userToken = getReplayUserToken();
  if (!userToken) {
    return null;
  }

  const userTokenInfo = tokenInfo(userToken);
  if (!userTokenInfo) {
    return null;
  }

  const exp = tokenExpiration(userToken);
  if (exp && exp > Date.now()) {
    // The current token hasn't expired yet so schedule a refresh and return it
    scheduleRefreshTimer(exp - Date.now());

    return userToken;
  }

  if (!getReplayRefreshToken()) {
    pingTelemetry("browser", "auth-no-refresh-token", {
      expirationDate: exp && new Date(exp).toISOString(),
      authId: userTokenInfo["sub"],
    });

    return;
  }

  try {
    return await refresh();
  } catch (e) {
    handleAuthRequestFailed(e);
  }
}

let refreshTimer: number | null = null;
function scheduleRefreshTimer(expiresInMs: number) {
  if (refreshTimer) {
    clearTimeout(refreshTimer);
  }

  // refresh a minute before token expiration
  refreshTimer = setTimeout(refresh, expiresInMs - (60 * 1000)) as any;
}

async function refresh() {
  const refresh_token = getReplayRefreshToken();
  try {
    const resp = await fetch(`https://${getAuthHost()}/oauth/token`, {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({
        audience: "https://api.replay.io",
        scope: "openid profile offline_access",
        grant_type: "refresh_token",
        client_id: getAuthClientId(),
        refresh_token,
      })
    });

    const json: any = await resp.json();

    if (json.error) {
      throw {
        id: "auth0-error",
        message: json.error
      };
    }

    if (!json.access_token) {
      throw {
        id: "no-access-token"
      };
    }

    if (!json.refresh_token) {
      throw {
        id: "no-refresh-token"
      };
    }

    setReplayRefreshToken(json.refresh_token);
    setReplayUserToken(json.access_token);

    scheduleRefreshTimer(json.expires_in * 1000);

    pingTelemetry("browser", "auth-refresh-success", {
      expirationDate: new Date(json.expires_in * 1000).toISOString(),
    });

    return json.access_token;
  } catch (e) {
    setReplayRefreshToken(null);
    setReplayUserToken(null);
    throw e;
  }
}

function openSignInPage() {
  const keyArray = new Uint8Array(32);
  crypto.getRandomValues(keyArray);
  const key = base64URLEncode(keyArray);
  const viewHost = getViewHost();
  const url = `${viewHost}/api/browser/auth?key=${key}`;

  openExternalBrowser(url);

  let timedOut = false;
  Promise.race([
    new Promise((_resolve, reject) => setTimeout(() => {
      timedOut = true;
      reject({id: "timeout"});
    }, 2 * 60 * 1000)),
    new Promise<void>(async (resolve, reject) => {
      try {
        while (!timedOut) {
          const resp = await queryAPIServer(`
            mutation CloseAuthRequest($key: String!) {
              closeAuthRequest(input: {key: $key}) {
                success
                token
              }
            }
          `, {
            key
          }, true);

          if (resp.errors) {
            if (resp.errors.length === 1 && resp.errors[0].message === "Authentication request does not exist") {
              await new Promise(resolve => setTimeout(resolve, 3000));
            } else {
              throw {
                id: "close-graphql-error",
                message: resp.errors.map((e: any) => e.message).filter(Boolean).join(", ")
              };
            }
          } else if (!resp.data.closeAuthRequest.token) {
            // there's no obvious reason this would occur but for completeness ...
            throw {
              id: "close-missing-token",
              message: JSON.stringify(resp)
            };
          } else {
            const refreshToken = resp.data.closeAuthRequest.token;
            pingTelemetry("browser", "auth-request-success", {});

            setReplayRefreshToken(refreshToken);

            // refresh the token immediately to exchange it for an access token
            // and acquire a new refresh token
            await refresh();

            resolve();
            break;
          }
        }
      } catch (e) {
        reject(e);
      }
    })
  ]).catch(e => {
    handleAuthRequestFailed(e, { clientKey: key })
  });
}

async function queryAPIServer(query: string, variables: Record<string, string> = {}, anonymous = false): Promise<any> {
  const token = anonymous ? null : (getReplayUserToken() || getOriginalApiKey());

  const headers: Record<string, string> = {
    "Content-Type": "application/json",
  };

  if (token) {
    headers["Authorization"] = `Bearer ${token}`;
  }

  const resp = await fetch(getAPIServer(), {
    method: "POST",
    headers,
    body: JSON.stringify({
      query,
      variables
    })
  });
  
  return resp.json();
}

function handleAuthRequestFailed(e: any, extra = {}) {
  const message = e?.id || "unexpected-internal-error";
  const errorMessage = e?.message;

  switch (message) {
    case 'timeout':
      showAuthenticationError("The request timed out before authentication completed. Please try again.")
      break;
    case 'unexpected-internal-error':
      showAuthenticationError("An unexpected error occurred. Support has been notified. You may try again or contact support@replay.io for help.");
      break;
  }

  const userToken = getReplayUserToken();
  const authId = userToken ? tokenInfo(userToken)?.["sub"] : null;

  pingTelemetry("browser", "auth-request-failed", {
    message,
    errorMessage,
    authId,
    ...extra,
  });
}

onSignInButtonClicked(openSignInPage);

// this will kick off a token refresh if necessary
validateUserToken();