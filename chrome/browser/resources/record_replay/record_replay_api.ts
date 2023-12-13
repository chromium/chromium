
export interface RecordReplayApiInterface {
  getEnv(key: string): Promise<string | null>;
  getBuildId(): Promise<string>;
  getReplayUserToken(): Promise<string | null>;
  setReplayUserToken(token: string | null): void;
  getReplayRefreshToken(): Promise<string | null>;
  setReplayRefreshToken(token: string | null): void;
  showAuthenticationError(message: string): void;
  onSignInButtonClicked(callback: () => void): void;
  openExternalBrowser(url: string): Promise<void>;
}

let gApiInterface: RecordReplayApiInterface | null = null;
const gApiAwaiters: Array<(iface: RecordReplayApiInterface) => void> = [];
export function setRecordReplayInterface(iface: RecordReplayApiInterface) {
  gApiInterface = iface;
  for (const awaiter of gApiAwaiters) {
    awaiter(iface);
  }
  gApiAwaiters.length = 0;
}
export function getRecordReplayInterface(): RecordReplayApiInterface {
  if (!gApiInterface) {
    throw new Error("RecordReplayApiInterface not set");
  }
  return gApiInterface;
}

function waitUntilRecordReplayInterface(): Promise<RecordReplayApiInterface> {
  if (gApiInterface) {
    return Promise.resolve(gApiInterface);
  }
  return new Promise<RecordReplayApiInterface>((resolve) => {
    gApiAwaiters.push(resolve);
  });
}

export async function getEnv(key: string): Promise<string | null> {
  return (await waitUntilRecordReplayInterface()).getEnv(key);
}

export async function getBuildId(): Promise<string> {
  return (await waitUntilRecordReplayInterface()).getBuildId();
}
export async function getReplayUserToken(): Promise<string | null> {
  return (await waitUntilRecordReplayInterface()).getReplayUserToken();
}
export async function setReplayUserToken(token: string | null): Promise<void> {
  (await waitUntilRecordReplayInterface()).setReplayUserToken(token);
}
export async function getReplayRefreshToken(): Promise<string | null> {
  return (await waitUntilRecordReplayInterface()).getReplayRefreshToken();
}
export async function setReplayRefreshToken(token: string | null): Promise<void> {
  (await waitUntilRecordReplayInterface()).setReplayRefreshToken(token);
}
export async function showAuthenticationError(message: string): Promise<void> {
  (await waitUntilRecordReplayInterface()).showAuthenticationError(message);
}
export async function openExternalBrowser(url: string): Promise<void> {
  (await waitUntilRecordReplayInterface()).openExternalBrowser(url);
}
export async function onSignInButtonClicked(callback: () => void): Promise<void> {
  (await waitUntilRecordReplayInterface()).onSignInButtonClicked(callback);
}
