### PODMAN

1. build podman

```bash
podman build -t mycppserver .
```

2. run podman container

```bash
podman run -d -p 5000:5000 -v ./uploads:/app/uploads:rw --name cpp-server-container mycppserver;
```

3. watch the container logs

```bash
podman logs -f cpp-server-container
```

4. stop podman container

```bash
podman stop cpp-server-container
```

5. remove podman container

```bash
podman rm cpp-server-container
```

6. to list all image

```bash
podman system df
```

7. to remove all image

```bash
podman system prune -af --volumes
```